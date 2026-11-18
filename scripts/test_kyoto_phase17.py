#!/usr/bin/env python3
"""Fast adversarial contracts for the Phase 17 evidence bridge."""
from __future__ import annotations

import csv
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_cbp_materialized_pairs as pairs
import kyoto_research_dashboard as dashboard


def write_csv(path: Path, columns: list[str], rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


class PairAuditTests(unittest.TestCase):
    def test_native_compatible_hashes(self):
        # Both hash mixers are transcribed independently from cbp_pressure.hpp.
        for a, b in ((0, 1), (1, 200), (4, 4294967295)):
            self.assertEqual(pairs.mix64((a << 32) | b) & pairs.MASK64,
                             pairs.mix64((a << 32) | b))
            self.assertEqual(pairs.mix32(a, b) & pairs.MASK32, pairs.mix32(a, b))

    def test_reject_duplicate_and_noncanonical_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            filename = Path(temp) / "pairs.bin"
            filename.write_bytes(struct.pack("<IIII", 0, 1, 0, 1))
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                pairs.audit_binary(filename, 4)
            filename.write_bytes(struct.pack("<II", 2, 1))
            with self.assertRaisesRegex(ValueError, "Invalid"):
                pairs.audit_binary(filename, 4)
            filename.write_bytes(struct.pack("<II", 0, 4))
            with self.assertRaisesRegex(ValueError, "Invalid"):
                pairs.audit_binary(filename, 4)
            filename.write_bytes(b"oops")
            with self.assertRaisesRegex(ValueError, "Truncated"):
                pairs.audit_binary(filename, 4)

    def test_cross_process_proof_and_fail_closed_fingerprint(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            binary = root / "frame-0.pairs"
            binary.write_bytes(struct.pack("<II", 0, 1) + struct.pack("<II", 2, 3))
            count, sum64, xor64, sum32, xor32 = pairs.audit_binary(binary, 4)
            self.assertEqual(count, 2)
            flat = {
                "scene": "sparse", "n": "4", "frame": "0", "seed": "17",
                "status": "complete", "selected_backend": "persistent_flat_3d_grid",
                "complete_query_ms": "2.0", "exact_pairs": str(count),
                "hash64_sum": str(sum64), "hash64_xor": str(xor64),
                "hash32_sum": str(sum32), "hash32_xor": str(xor32),
            }
            emitted = {
                "scene": "sparse", "n": "4", "frame": "0", "seed": "17",
                "status": "VERIFIED_WRITTEN", "sink": "file",
                "first_count_digest_matches": "1",
                "exact_candidate_pairs": str(count), "real_pair_bytes": "16",
                "total_stream_ms": "3.0", "output_file": str(binary),
            }
            flatfile = root / "flat.csv"
            streamfile = root / "emit.csv"
            write_csv(flatfile, list(flat), [flat])
            write_csv(streamfile, list(emitted), [emitted])
            proof = pairs.verify(flatfile, streamfile)
            self.assertEqual(proof["total_pair_bytes"], 16)
            self.assertTrue(proof["all_pairs_uniquely_checked"])
            flat["hash32_xor"] = str(xor32 ^ 1)
            write_csv(flatfile, list(flat), [flat])
            with self.assertRaisesRegex(ValueError, "disagree"):
                pairs.verify(flatfile, streamfile)


class DashboardTests(unittest.TestCase):
    def test_original_methods_present_and_comparable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            methods = sorted(dashboard.REQUIRED)
            rows = []
            for method in methods:
                for frame in range(6):
                    rows.append({
                        "backend": method, "frame": frame,
                        "ms": 3 if method == "global_guarded_exact" else 2,
                        "pairs": 5, "pair_hash": "123456", "misses": 0,
                    })
            filename = root / "replays" / "cloth-baseline-evaluated.csv"
            write_csv(filename, list(rows[0]), rows)
            data = dashboard.material_results(root, 2)
            self.assertEqual(data["trajectories"], 1)
            self.assertEqual(set(data["methods"]), dashboard.REQUIRED)
            self.assertAlmostEqual(
                data["methods"]["sap_rebuild"]["guard_speedup_geomean"], 1.5)
            # A missing method is never silently ranked as if it were tested.
            write_csv(filename, list(rows[0]),
                      [row for row in rows if row["backend"] != "online_cost_aware"])
            with self.assertRaisesRegex(ValueError, "Missing historical"):
                dashboard.material_results(root, 2)

    def test_phase16_framewise_parity(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            base = {"scene": "burst", "n": "1024", "seed": "42",
                    "status": "complete", "selected_backend": "persistent_flat_3d_grid",
                    "exact_pairs": "12", "hash64_sum": "33", "hash64_xor": "44",
                    "hash32_sum": "55", "hash32_xor": "66",
                    "complete_query_ms": "7.0", "query_ms": "3.0",
                    "workers_used": "1", "thread_limit": "1"}
            def make(threads, corrupt=False):
                records = []
                for frame in range(3):
                    r = dict(base, frame=str(frame), thread_limit=str(threads),
                             workers_used=str(threads),
                             complete_query_ms="5.0" if threads > 1 else "7.0")
                    if corrupt and frame == 1:
                        r["hash64_sum"] = "999"
                    records.append(r)
                write_csv(root / f"burst-n1024-f3-threads{threads}.csv",
                          list(records[0]), records)
            make(1)
            make(4)
            result = dashboard.synthetic_results(root)
            self.assertAlmostEqual(
                result["scenes"]["burst-n1024-f3"]["methods"]["4"]
                ["speedup_vs_same_scene_1thread"], 1.4)
            make(4, corrupt=True)
            with self.assertRaisesRegex(ValueError, "mismatch"):
                dashboard.synthetic_results(root)


if __name__ == "__main__":
    unittest.main(verbosity=2)
