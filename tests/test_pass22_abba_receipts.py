#!/usr/bin/env python3
"""Source-receipt-only parser regression for the four-arm native ABBA duel.

These minimal fixtures are deliberately synthetic parser controls.
They are NEVER original physical or performance evidence.
"""
from __future__ import annotations
import csv
import importlib.util
import pathlib
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "kyoto_abba_report",ROOT/"scripts/report_pass22_abba.py")
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


def fixture(path:pathlib.Path,*,wrong_order=False,wrong_pair=False)->None:
    with path.open("w",encoding="utf-8",newline="") as file:
        writer=csv.DictWriter(file,fieldnames=module.FIELDS)
        writer.writeheader()
        orders=sorted(module.VALID_ORDERS)
        for trial in range(4):
            for scene,steps,regime in (
                (0,(1,2),"routine"),
                (4,(5001,5002),"uncoupled_multibody")):
                for j,step in enumerate(steps):
                    row={name:"1" for name in module.FIELDS}
                    row.update({
                        "trial":str(trial),"step":str(step),
                        "scene_id":str(scene),"regime":regime,
                        "source_vertices":"123",
                        "source_tetrahedra":"144",
                        "oracle_candidates":"9",
                        "cold_scene_frame":"1" if j==0 else "0",
                        "order_code":orders[(trial+j)%4],
                        "selective_A1_wall_ns":"100",
                        "selective_A2_wall_ns":"100",
                        "ipc_B1_wall_ns":"110",
                        "ipc_B2_wall_ns":"110",
                        "selective_A1_pairs":"9",
                        "selective_A2_pairs":"9",
                        "ipc_B1_pairs":"9",
                        "ipc_B2_pairs":"9",
                        "oracle_parity":"1",
                    })
                    if wrong_order and trial==0 and step==1:
                        row["order_code"]="A1-A2-B1-B2"
                    if wrong_pair and trial==0 and step==1:
                        row["ipc_B2_pairs"]="0"
                    writer.writerow(row)


class FourArmReceiptTests(unittest.TestCase):
    def test_complete_counterbalanced_native_schema(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/"synthetic_parser_fixture.csv"
            fixture(path)
            rows=module.load(path)
            report=module.summarize(rows)
            self.assertEqual(report["total_source_rows"],16)
            self.assertEqual(report["warm_rows"],8)
            self.assertEqual(report["cold_rows"],8)
            self.assertAlmostEqual(
                report["warm_steady_state"]["official_over_selective_ratio"],1.1)
            self.assertEqual(len(report["per_source_scene"]),2)
            self.assertIn("A_outer",report["per_order"])
            self.assertIn("B_outer",report["per_order"])
    def test_wrong_method_order_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/"wrong_order.csv"
            fixture(path,wrong_order=True)
            with self.assertRaisesRegex(ValueError,"ABBA order"):
                module.load(path)
    def test_missing_native_candidate_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/"wrong_pairs.csv"
            fixture(path,wrong_pair=True)
            with self.assertRaisesRegex(ValueError,"candidate mismatch"):
                module.load(path)


if __name__=="__main__":
    unittest.main()
