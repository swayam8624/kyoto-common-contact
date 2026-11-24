#!/usr/bin/env python3
"""Only parser fixtures: NEVER original FEM source evidence."""
from __future__ import annotations
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "pass25_report",ROOT/"scripts/report_pass25_traversal_abba.py")
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


def fixture(path:Path,*,bad_pair:bool=False,bad_clock:bool=False)->None:
    with path.open("w",encoding="utf-8",newline="") as f:
        writer=csv.DictWriter(f,fieldnames=module.FIELDS)
        writer.writeheader()
        for trial in range(4):
            for scene in range(2):
                for j in range(4):
                    row={k:"1" for k in module.FIELDS}
                    row.update({
                        "source_kind":"original_physically_integrated_FEM",
                        "scene":str(scene),
                        "step":str((scene+1)*1000+j),
                        "trial":str(trial),
                        "order_code":module.ORDERS[(trial+j)%4],
                        "cold_epoch":"1" if j==0 else "0",
                        "source_tetrahedra":"288",
                        "source_vertices":"157",
                        "physical_time_s":str(.001*(j+1)),
                        "source_contact_impulses":"2",
                        "accepted_physical_leaves":"1",
                        "oracle_candidates":"19",
                        "dirty_vertices":"100",
                        "span_policy_enabled_A1":"1",
                        "span_policy_enabled_A2":"1",
                        "A1_span_paid_ns":"100",
                        "A2_span_paid_ns":"100",
                        "B1_incumbent_paid_ns":"120",
                        "B2_incumbent_paid_ns":"120",
                        "A1_pairs":"19",
                        "A2_pairs":"19",
                        "B1_pairs":"19",
                        "B2_pairs":"19",
                    })
                    if bad_pair and scene==0 and trial==0 and j==0:
                        row["A2_pairs"]="0"
                    if bad_clock and scene==1 and trial==1 and j==0:
                        row["B1_incumbent_paid_ns"]="0"
                    writer.writerow(row)


class ABBAReceipts(unittest.TestCase):
    def test_valid_four_instance_synthetic_parser_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"parser_fixture_only.csv"
            fixture(path)
            rows=module.load(path)
            report=module.summary(rows)
            self.assertEqual(report["total_rows"],32)
            self.assertEqual(report["warm_rows"],24)
            self.assertAlmostEqual(
                report["warm"]["incumbent_over_optimized"],1.2)
            self.assertTrue(report["all_trials_favor_optimized"])
            self.assertTrue(report["span_descent_enabled"])

    def test_failed_candidate_parity_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"fixture_bad.csv"
            fixture(path,bad_pair=True)
            with self.assertRaisesRegex(ValueError,"candidate oracle mismatch"):
                module.load(path)

    def test_invalid_native_paid_clock_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"fixture_bad_clock.csv"
            fixture(path,bad_clock=True)
            with self.assertRaisesRegex(ValueError,"nonpositive paid native clock"):
                module.load(path)


if __name__=="__main__":unittest.main()
