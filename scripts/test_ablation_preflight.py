#!/usr/bin/env python3
"""Regression tests for failed-build/stale-executable ablation protection."""
import csv
import tempfile
import unittest
from pathlib import Path

from scripts.run_kyoto_controller_ablation import (
    REPLAY_ABI, verify_replayer, verify_replay_csv, choose_combos,
)


class AblationPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="kyoto-preflight-")
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.bin=self.root/"ccp_material_replay"

    def write_binary(self, output, returncode=0):
        self.bin.write_text(
            "#!/usr/bin/env python3\n"
            f"print({output!r})\n"
            f"raise SystemExit({returncode})\n",encoding="utf-8")
        self.bin.chmod(0o755)

    def test_rejects_old_replay_binary_before_any_data_generation(self):
        self.write_binary("usage: ccp_material_replay INPUT OUTPUT",1)
        with self.assertRaisesRegex(RuntimeError,"outdated"):
            verify_replayer(self.bin,self.root)

    def test_accepts_current_capability_signature(self):
        self.write_binary(
            f"{REPLAY_ABI} sap_persistent_coherent ml_cost_aware_conservative "
            "guard_sap_measured_probe guard_sap_bounded_trial "
            "hybrid_probed hybrid_switched field_events randomized_method_order "
            "global_guarded_aa_control bvh_refit_exact bvh_adaptive_exact "
            "grid_spatial_hash_exact delta_spatial_join_exact "
            "patch_certificate_fixed patch_certificate_signal "
            "unified_certified_hierarchy_v0 "
            "ml_bookkeeping_ms ml_wrapper_ms")
        verify_replayer(self.bin,self.root)

    def test_rejects_csv_missing_timing_columns(self):
        report=self.root/"old.csv"
        with report.open("w",newline="",encoding="utf-8") as f:
            w=csv.writer(f)
            w.writerow(["backend","frame","ms"])
            w.writerow(["ml_cost_aware",0,1.0])
        with self.assertRaisesRegex(RuntimeError,"ml_bookkeeping_ms"):
            verify_replay_csv(report)

    def test_rejects_csv_without_persistent_sap(self):
        report=self.root/"old-with-timing.csv"
        from scripts.run_kyoto_controller_ablation import REQUIRED_COLUMNS
        headers=sorted(REQUIRED_COLUMNS)
        with report.open("w",newline="",encoding="utf-8") as f:
            w=csv.DictWriter(f,fieldnames=headers)
            w.writeheader()
            for policy in ("sap_rebuild","global_guarded_exact",
                           "ml_cost_aware","ml_cost_aware_conservative"):
                w.writerow({"backend":policy,"frame":0,"ms":1.0})
        with self.assertRaisesRegex(RuntimeError,"sap_persistent_coherent"):
            verify_replay_csv(report)

    def test_balanced_selection_covers_every_cell_and_independent_seeds(self):
        seeds=[1709,1801,1907,2003]
        materials=["cloth","spring"]
        regimes=["impact","relax","rapid"]
        budgets=[128,512,2048]
        a=choose_combos(seeds,materials,regimes,budgets,20261117,18,"balanced")
        self.assertEqual(a,choose_combos(seeds,materials,regimes,budgets,20261117,18,"balanced"))
        self.assertEqual(len(a),18)
        self.assertEqual(len({(m,r,n) for _,m,r,n in a}),18)
        b=choose_combos(seeds,materials,regimes,budgets,20261117,36,"balanced")
        for m in materials:
            for r in regimes:
                for n in budgets:
                    actual=[seed for seed,mat,reg,budget in b
                            if (mat,reg,budget)==(m,r,n)]
                    self.assertEqual(len(actual),2)
                    self.assertEqual(len(set(actual)),2)
        self.assertEqual(len(choose_combos(seeds,materials,regimes,budgets,
                                          20261117,0,"balanced")),72)

    def test_repeat_schedule_has_distinct_stable_orders(self):
        import hashlib
        # The same snapshot is benchmarked repeatedly with separate,
        # deterministic shuffled-method order seeds.
        stem="actual-cloth-impact-n512-seed3001"
        keys=[int.from_bytes(hashlib.sha256(
            f"20261118|{stem}|{r}".encode()).digest()[:8],"little") for r in range(3)]
        self.assertEqual(len(set(keys)),3)

    def test_accepts_complete_replay_schema(self):
        report=self.root/"complete.csv"
        from scripts.run_kyoto_controller_ablation import REQUIRED_COLUMNS, REQUIRED_POLICIES
        headers=sorted(REQUIRED_COLUMNS)
        with report.open("w",newline="",encoding="utf-8") as f:
            w=csv.DictWriter(f,fieldnames=headers)
            w.writeheader()
            for policy in sorted(REQUIRED_POLICIES):
                w.writerow({"backend":policy,"frame":0,"ms":1.0})
        verify_replay_csv(report)


if __name__=="__main__":
    unittest.main()
