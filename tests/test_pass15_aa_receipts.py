#!/usr/bin/env python3
"""Regression gates for original Pass 15 A/A timing receipts.

Fixtures here are parser tests only. They are NOT physical measurements,
and no generated fixture can be used as research performance evidence.
"""
from __future__ import annotations
import csv
import importlib.util
import pathlib
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "pass15_receipt_report", ROOT/"scripts/report_pass15_official_ipc.py")
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


def fixture(path, *, bad_parity=False, bad_timing=False):
    fields=list(module.REQUIRED)
    with path.open("w",encoding="utf-8",newline="") as output:
        writer=csv.DictWriter(output,fieldnames=fields)
        writer.writeheader()
        for trial in (0,1):
            for frame,regime in ((1,"routine"),(2,"fast_adaptive")):
                row={field:"1" for field in fields}
                row.update({
                    "repeat":str(trial),"step":str(frame),
                    "regime":regime,"scene_id":str(frame),
                    "first_method":"ipc",
                    "first_internal_hierarchy_method":"cached",
                    "aa_first_method":"A" if (trial+frame)%2 else "B",
                    "aa_exact_candidate_parity":"0" if bad_parity else "1",
                    "aa_first_hierarchy_ns":"0" if bad_timing else "100",
                    "aa_second_hierarchy_ns":"100",
                    "reference_pairs":"5",
                    "maintained_hierarchy_pairs":"5",
                    "frontier_pairs":"5",
                    "uncached_hierarchy_pairs":"5",
                    "official_ipc_pairs":"5",
                    "cached_ipc_pairs":"5",
                    "official_missing":"0","official_extra":"0",
                })
                writer.writerow(row)


class NativeAAReceiptTests(unittest.TestCase):
    def test_source_parser_and_aa_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            path=pathlib.Path(directory)/"synthetic_parser_test_only.csv"
            fixture(path)
            rows=module.read_data(path)
            summary=module.summarize(rows)
            aa=summary["aa_control"]
            self.assertEqual(aa["first_A_rows"],2)
            self.assertEqual(aa["first_B_rows"],2)
            self.assertEqual(aa["A_over_B_total_time"],1)
            self.assertEqual(summary["repetitions"],2)

    def test_aa_parity_failure_rejects_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            path=pathlib.Path(directory)/"invalid.csv"
            fixture(path,bad_parity=True)
            with self.assertRaisesRegex(ValueError,"A/A correctness gate"):
                module.read_data(path)

    def test_nonpositive_clock_rejects_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            path=pathlib.Path(directory)/"invalid_clock.csv"
            fixture(path,bad_timing=True)
            with self.assertRaisesRegex(ValueError,"A/A timing"):
                module.read_data(path)


if __name__=="__main__":
    unittest.main()
