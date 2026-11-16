#!/usr/bin/env python3
"""Regression tests for failed-build/stale-executable ablation protection."""
import csv
import tempfile
import unittest
from pathlib import Path

from scripts.run_kyoto_controller_ablation import (
    REPLAY_ABI, verify_replayer, verify_replay_csv,
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
            "guard_sap_measured_probe hybrid_probed hybrid_switched "
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
