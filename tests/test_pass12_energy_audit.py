#!/usr/bin/env python3
"""Synthetic unit controls for the Pass 12 source-data energy auditor.

These tiny synthetic rows validate accounting only. They are NOT physics
measurements, and must never enter a source-data evidence archive.
"""
import importlib.util
import math
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "audit_pass12_mesh_energy.py"
spec = importlib.util.spec_from_file_location("mesh_energy_audit", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def row(step, kinetic, elastic, gravity, impulses=0):
    return dict(
        step=float(step), physical_time_s=step * 0.00075,
        actual_accepted_time_s=0.00075 if step else 0.0,
        accepted_leaves=1.0 if step else 0.0,
        rejected_geom=0.0, rejected_tetra=0.0,
        active_contact_pairs=1.0 if impulses else 0.0,
        face_impulse_updates=float(impulses), normal_impulse_ns=0.0,
        kinetic_j=float(kinetic), elastic_j=float(elastic), gravity_j=float(gravity),
        finite_triangle_min_distance_m=0.001, min_tetra_j=0.9,
    )


class EnergySourceAuditTests(unittest.TestCase):
    def test_telescopes_and_groups_true_contact_receipts(self):
        rows = [
            row(0, 10, 2, 3),
            row(1, 8, 2.5, 3.25),
            row(2, 6, 2.7, 3.5, impulses=7),
        ]
        details, report = module.analyze(rows)
        self.assertEqual(report["physical_steps"], 2)
        self.assertEqual(report["regimes"]["contact"]["steps"], 1)
        self.assertEqual(report["impulse_updates"], 7)
        self.assertTrue(math.isclose(report["delta_mechanical_j"], -2.8))
        self.assertAlmostEqual(report["telescoping_residual_j"], 0.0)
        self.assertEqual(details[-1]["regime"], "contact")

    def test_rejects_fabricated_elapsed_time(self):
        rows = [row(0, 10, 2, 3), row(1, 9, 2, 3)]
        rows[1]["actual_accepted_time_s"] = 0.0005
        with self.assertRaisesRegex(ValueError, "elapsed-time"):
            module.analyze(rows)

    def test_rejects_missing_step_and_nonpositive_jacobian(self):
        with self.assertRaisesRegex(ValueError, "non-contiguous"):
            module.analyze([row(0, 10, 2, 3), row(2, 9, 2, 3)])
        # Source CSV validator handles unsafe jacobians; analyze's direct
        # helper deliberately expects already validated input.
        bad = row(1, 9, 2, 3)
        bad["accepted_leaves"] = 0
        with self.assertRaisesRegex(ValueError, "no accepted"):
            module.analyze([row(0, 10, 2, 3), bad])


if __name__ == "__main__":
    unittest.main()
