#!/usr/bin/env python3
"""Offline parser/regression tests; fixtures are invented for tests ONLY.

No fixture values are research evidence. This test confirms that observable
charts use values from verified CSVs and fail on changed/missing raw sources.
"""
from __future__ import annotations

import csv
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import rbc_benchmark_observatory as obs


class ObservatoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.material = self.root / "material_csv"
        self.material.mkdir()
        self.trace = self.material / "example.csv"
        self.manifest = self.root / "RBC_RESULTS.json"
        self.research_rows = []
        for frame in range(3):
            for method_i, method in enumerate(obs.MATERIAL_METHODS):
                self.research_rows.append({
                    "method": method, "frame": frame,
                    "query_ms": 0.5 + method_i * 0.2,
                    "after_warmup": int(frame >= 1),
                    "pairs": 17, "pair_fnv64": 123456789,
                })
        self.write_csv(self.trace, self.research_rows)
        means = {m: .5 + i * .2 for i, m in enumerate(obs.MATERIAL_METHODS)}
        self.manifest.write_text(json.dumps({
            "schema": "kyoto-rbc-stage2-real-native-evidence-v1",
            "native_material_scenes": [{
                "material": "cloth", "regime": "shear", "budget": 128,
                "actual_n": 128,
                "result": {
                    "source": str(self.trace),
                    "sha256": obs.sha256(self.trace),
                    "warmup": 1, "per_method_mean_ms": means,
                    "all_five_pair_streams_equal_fresh_sap_on_every_frame": True,
                },
            }],
            "coupled_physical_scenes": [],
            "warnings": ["Invented test-only data; not a benchmark"],
        }), encoding="utf-8")

    @staticmethod
    def write_csv(path, rows):
        with path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)

    def test_accepted_source_creates_measured_offline_charts(self):
        research = obs.read_research(self.manifest)
        self.assertEqual(len(research["scenes"]), 1)
        table = obs.aggregate(research["scenes"])
        self.assertEqual(len(table), len(obs.MATERIAL_METHODS))
        payload = {"research": research, "proof": None,
                   "summary": table,
                   "physical_methods": obs.PHYSICAL_METHODS}
        document = obs.html_document(payload)
        self.assertIn('id="surface"', document)
        self.assertIn('id="heatmap"', document)
        self.assertIn("guard_fixed_exact", document)
        self.assertNotIn("__DATA__", document)

    def test_verified_coupled_solver_source_drives_frame_chart(self):
        coupled = self.root / "coupled_csv"
        coupled.mkdir()
        source = coupled / "free.csv"
        rows = []
        for method in obs.PHYSICAL_METHODS:
            for frame in range(10):
                rows.append({
                    "policy": method, "frame": frame,
                    "step_ms": 2.5, "broad_ms": .4, "narrow_ms": .8,
                    "self_contacts": 3, "sphere_contacts": 2,
                    "self_crossings": 0, "max_stretch": 1.15,
                    "oracle_misses": 0,
                })
        self.write_csv(source, rows)
        doc = json.loads(self.manifest.read_text(encoding="utf-8"))
        doc["coupled_physical_scenes"] = [{
            "scenario": "free",
            "result": {
                "source": str(source), "sha256": obs.sha256(source),
                "all_sap_reference_particle_states_equal": True,
                "solver_mean_full_step_ms_by_policy": {
                    m: 2.5 for m in obs.PHYSICAL_METHODS},
            },
        }]
        self.manifest.write_text(json.dumps(doc), encoding="utf-8")
        parsed = obs.read_research(self.manifest)
        self.assertEqual(len(parsed["physical"]), 1)
        free = parsed["physical"][0]
        self.assertEqual(free["frames"], 10)
        self.assertEqual(len(free["series"]["rbc_flat"]), 10)
        self.assertEqual(free["series"]["rbc_flat"][0]["sphere_contacts"], 2)
        self.assertEqual(free["means"]["rbc_flat"], 2.5)

    def test_offline_embedded_javascript_parses_with_node(self):
        if not shutil.which("node"):
            self.skipTest("Node absent; CI installs Node 22 explicitly")
        research = obs.read_research(self.manifest)
        doc = obs.html_document({
            "research": research, "proof": None,
            "summary": obs.aggregate(research["scenes"]),
            "physical_methods": obs.PHYSICAL_METHODS,
        })
        script = doc.rsplit("<script>", 1)[1].split("</script>", 1)[0]
        run = subprocess.run(["node", "--check"], input=script,
                             text=True, capture_output=True, check=False)
        self.assertEqual(run.returncode, 0, run.stderr)

    def test_changed_pair_fingerprint_rejected_even_with_valid_manifest_hash(self):
        self.research_rows[0]["pair_fnv64"] = 99
        self.write_csv(self.trace, self.research_rows)
        doc = json.loads(self.manifest.read_text(encoding="utf-8"))
        doc["native_material_scenes"][0]["result"]["sha256"] = obs.sha256(self.trace)
        self.manifest.write_text(json.dumps(doc), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "pair identity differs"):
            obs.read_research(self.manifest)

    def test_tampering_source_hash_rejected(self):
        self.trace.write_text(self.trace.read_text(encoding="utf-8")+"bogus\n",
                              encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "hash changed"):
            obs.read_research(self.manifest)

    def test_real_policy_csv_contract_and_failed_oracle(self):
        proof = self.root / "NATIVE_FIVE_POLICY_SOLVER.csv"
        records = []
        for method in obs.PHYSICAL_METHODS:
            for frame in range(10):
                records.append({
                    "policy": method, "frame": frame, "step_ms": 2.5,
                    "broad_ms": .3, "narrow_ms": .6,
                    "self_contacts": 3, "sphere_contacts": 2,
                    "geometric_self_intersections": 0,
                    "max_position_diff_sap": 0, "oracle_misses": 0,
                })
        self.write_csv(proof, records)
        parsed = obs.read_proof(proof)
        self.assertEqual(parsed["frames"], 10)
        self.assertEqual(len(parsed["policies"]), 5)
        records[-1]["oracle_misses"] = 1
        self.write_csv(proof, records)
        with self.assertRaisesRegex(ValueError, "oracle misses"):
            obs.read_proof(proof)


if __name__ == "__main__":
    unittest.main()
