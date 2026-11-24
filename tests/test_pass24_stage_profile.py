#!/usr/bin/env python3
"""Pass24 report parser fixtures; NOT simulated contact evidence."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "pass24_report",ROOT/"scripts/report_pass24_stage_profile.py")
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


def fixture(path:Path,bad_sum=False,bad_pair=False):
    with path.open("w",encoding="utf-8",newline="") as f:
        writer=csv.DictWriter(f,fieldnames=module.FIELDS)
        writer.writeheader()
        for trial in range(2):
            for scene in range(3):
                for step in (1,2):
                    for variant in ("full","selective"):
                        row={key:"1" for key in module.FIELDS}
                        row.update({
                            "source_kind":"original_solver_integrated_FEM",
                            "trial":str(trial),"scene":str(scene),
                            "regime":"routine","step":str(step),
                            "variant":variant,
                            "source_tetrahedra":"288",
                            "source_vertices":"157",
                            "source_impulses":"0",
                            "source_candidate_pairs":"37",
                            "cold_epoch":"1" if step==1 else "0",
                            "dirty_vertices":"100",
                            "topology_signature_ns":"20",
                            "swept_feature_boxes_ns":"30",
                            "cold_build_or_warm_refit_ns":"40",
                            "vf_ee_traversal_ns":"50",
                            "candidate_normalize_ns":"10",
                            "total_ns":"150"
                        })
                        if bad_sum and trial==0 and scene==0 and step==1:
                            row["total_ns"]="149"
                        if bad_pair and trial==0 and scene==0 and step==1 and variant=="selective":
                            continue
                        writer.writerow(row)


class NativeProfileParser(unittest.TestCase):
    def test_stage_partition_complete(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"fixture_only.csv"
            fixture(path)
            data=module.load(path)
            report=module.summary(data)
            self.assertEqual(report["warm_rows"],12)
            self.assertEqual(report["cold_rows"],12)
            self.assertAlmostEqual(
                sum(report["by_variant"]["selective"]["stage_fraction"].values()),1)

    def test_malformed_stage_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"bad.csv"
            fixture(path,bad_sum=True)
            with self.assertRaisesRegex(ValueError,"do not reconcile"):
                module.load(path)

    def test_missing_control_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"bad.csv"
            fixture(path,bad_pair=True)
            with self.assertRaisesRegex(ValueError,"both methods"):
                module.load(path)


if __name__=="__main__":unittest.main()
