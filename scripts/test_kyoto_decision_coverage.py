#!/usr/bin/env python3
"""Regression: randomized original-policy order cannot change exact pairs.

Also covers immutable KYOTOM01 rigid-translation diagnostics, exact
frame/seed/size receipts and a real threshold-eligible material replay.
The frozen V4 selector is not retuned or mocked.
"""
from __future__ import annotations

import argparse
import csv
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import (
    BOX, HEADER, check_ordered_replay, random_order_seed,
    snapshot_metadata, translated_diagnostic,
)
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score


def execute(*args: object) -> None:
    completed = subprocess.run([str(a) for a in args], capture_output=True,
                               text=True, timeout=90, check=False)
    if completed.returncode:
        raise RuntimeError(f"Command failed: {args}\n"
                           f"{completed.stdout[-1500:]}\n{completed.stderr[-1500:]}")


class DecisionCoverageRegressions(unittest.TestCase):
    exporter = ""
    replayer = ""

    def test_seed_is_reproducible_and_case_sensitive(self):
        a = random_order_seed("cloth", "rapid", 512, 3203, "natural")
        self.assertEqual(a, random_order_seed("cloth", "rapid", 512, 3203, "natural"))
        self.assertNotEqual(a, random_order_seed("cloth", "rapid", 512, 3203,
                                                 "rigid_translation_diagnostic"))

    def test_real_randomized_order_and_exactness(self):
        with tempfile.TemporaryDirectory(prefix="kyoto-decision-test-") as name:
            root = Path(name)
            original = root / "cloth.kmcb"
            execute(self.exporter, original, "cloth", 512, 12, 61749, "rapid")
            metadata = snapshot_metadata(original)
            self.assertGreaterEqual(metadata["n"], 384)
            self.assertEqual(metadata["frames"], 12)
            reports = []
            orders = []
            for seed in (27, 137):
                output = root / f"order{seed}-evaluated.csv"
                execute(self.replayer, original, output, 0, "--order-seed", seed)
                scored = score(output, skip=2)
                self.assertIn("guard_sap_bounded_trial", scored["methods"])
                self.assertIn("online_cost_aware", scored["methods"])
                self.assertIn("global_guarded_exact", scored["methods"])
                order = check_ordered_replay(output, seed)
                self.assertEqual(len(order), len(scored["methods"]))
                orders.append(order)
                reports.append(scored)
            self.assertNotEqual(orders[0], orders[1],
                                "Policy execution order did not change")
            # This must NOT assert equal timings. Only exact collision-pair
            # parity is required between orders, which the strict scorer
            # checks against SAP independently for every frame.
            self.assertEqual(set(reports[0]["methods"]), set(reports[1]["methods"]))
            # A malformed order seed must be rejected BEFORE an evidence
            # CSV can be created. Silent partial parsing breaks provenance.
            for malformed in ("27garbage", "-1", "4294967296", "0"):
                nonexistent = root / ("invalid-" + malformed + ".csv")
                response = subprocess.run(
                    [self.replayer, str(original), str(nonexistent), "0",
                     "--order-seed", malformed],
                    capture_output=True, text=True, timeout=10, check=False)
                self.assertNotEqual(response.returncode, 0)
                self.assertFalse(nonexistent.exists())

    def test_translation_preserves_pair_geometry(self):
        with tempfile.TemporaryDirectory(prefix="kyoto-motion-test-") as name:
            root = Path(name)
            original, transformed = root / "original.kmcb", root / "translated.kmcb"
            execute(self.exporter, original, "cloth", 512, 12, 3203, "rapid")
            translated_diagnostic(original, transformed)
            self.assertEqual(snapshot_metadata(original),
                             snapshot_metadata(transformed))
            with original.open("rb") as first, transformed.open("rb") as second:
                first.read(HEADER.size)
                second.read(HEADER.size)
                for frame in range(12):
                    displacement = .30 if frame % 2 else 0.
                    for _ in range(snapshot_metadata(original)["n"]):
                        box = BOX.unpack(first.read(BOX.size))
                        moved = BOX.unpack(second.read(BOX.size))
                        for a, b in zip(box, moved):
                            self.assertAlmostEqual(
                                b - a, displacement, delta=0.00002)
            source_csv = root / "original-evaluated.csv"
            shifted_csv = root / "translated-evaluated.csv"
            execute(self.replayer, original, source_csv, 0, "--order-seed", 19)
            execute(self.replayer, transformed, shifted_csv, 0, "--order-seed", 29)
            def signatures(path):
                with path.open(newline="", encoding="utf-8") as data:
                    return {
                        int(row["frame"]): (int(row["pairs"]), row["pair_hash"])
                        for row in csv.DictReader(data)
                        if row["backend"] == "sap_rebuild"
                    }
            self.assertEqual(signatures(source_csv), signatures(shifted_csv),
                             "Rigid translation changed current-AABB candidate truth")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exporter", type=Path)
    parser.add_argument("replayer", type=Path)
    args = parser.parse_args()
    DecisionCoverageRegressions.exporter = str(args.exporter.resolve())
    DecisionCoverageRegressions.replayer = str(args.replayer.resolve())
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DecisionCoverageRegressions)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        raise SystemExit(1)


if __name__ == "__main__":
    main()
