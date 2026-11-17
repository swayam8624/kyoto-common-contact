#!/usr/bin/env python3
"""Unit tests for repeat aggregation, seed-block bootstrap, and oracle errors."""
import csv
import tempfile
import unittest
from pathlib import Path

from scripts.analyze_kyoto_rumble import analyze, write_report

METHODS=("sap_rebuild","sap_persistent_coherent","global_guarded_exact",
         "global_guarded_aa_control","guard_sap_bounded_trial",
         "patch_certificate_signal")
FIELDS=("backend","frame","ms","n","pairs","pair_hash","misses","builds",
        "reuses","field_events","f8")


class RumbleAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="rumble-pattern-unit-")
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)

    def fixture(self,seed,repeat,bad=False):
        path=self.root/f"actual-cloth-impact-n128-seed{seed}-rep{repeat}-evaluated.csv"
        with path.open("w",newline="",encoding="utf-8") as file:
            writer=csv.DictWriter(file,fieldnames=FIELDS)
            writer.writeheader()
            for backend in METHODS:
                for frame in range(12):
                    baseline={
                        "sap_rebuild":3.,
                        "sap_persistent_coherent":2.5,
                        "global_guarded_exact":2.,
                        "global_guarded_aa_control":2.,
                        "guard_sap_bounded_trial":1.,
                        "patch_certificate_signal":1.5,
                    }[backend]
                    writer.writerow({
                        "backend":backend,"frame":frame,
                        "ms":baseline if repeat==1 else baseline*1.1,
                        "n":128,"pairs":4,
                        "pair_hash":"INVALID" if bad and backend=="patch_certificate_signal"
                                            and frame==3 else "123456",
                        "misses":0,"builds":int(frame%2==0) if backend==
                                "global_guarded_exact" else 0,
                        "reuses":0,"field_events":int(frame%3==0),
                        "f8":0.1,
                    })
        return path

    def test_median_of_repeats_and_seed_blocked_ci(self):
        files=[self.fixture(seed,repeat)
               for seed in (3101,3109) for repeat in (1,2,3)]
        report=analyze(files,warmup=2,bootstrap=100,seed=100)
        self.assertEqual(report["protocol"]["unique_cells"],2)
        self.assertEqual(report["protocol"]["original_csv_files"],6)
        self.assertEqual([c["repeats"] for c in report["cells"]],[3,3])
        champion=next(x for x in report["leaderboard"]
                      if x["method"]=="guard_sap_bounded_trial")
        self.assertAlmostEqual(champion["geometric_speedup_vs_guard"],2.)
        self.assertEqual(champion["fastest_scene_count"],2)
        self.assertEqual(champion["mean_scene_wins_vs_guard"],2)
        self.assertEqual(len(champion["seed_block_bootstrap_95pct"]),2)
        self.assertAlmostEqual(
            report["guard_aa"]["geometric_speedup_vs_guard"],1.)
        write_report(report,self.root/"results")
        self.assertTrue((self.root/"results"/"RUMBLE_PATTERNS.json").is_file())
        self.assertTrue((self.root/"results"/"RUMBLE_PATTERNS.md").is_file())

    def test_mismatched_exact_pair_hash_rejected(self):
        with self.assertRaisesRegex(ValueError,"Wrong exact AABB pairs"):
            analyze([self.fixture(3101,1,bad=True)],warmup=2,bootstrap=100)

if __name__=="__main__": unittest.main()
