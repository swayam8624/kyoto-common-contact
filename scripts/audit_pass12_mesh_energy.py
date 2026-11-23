#!/usr/bin/env python3
"""Pass 12: source-backed mechanical-energy accounting for rigid-mesh FEM.

Reads the ORIGINAL native CSV emitted by kyoto_rigid_mesh_fem_tests.
This is a diagnostic, NOT a calibrated physical dissipation model.
Standard library only; never synthesizes solver measurements.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path

FIELDS = (
    "step", "physical_time_s", "accepted_leaves", "rejected_geom",
    "rejected_tetra", "actual_accepted_time_s", "active_contact_pairs",
    "face_impulse_updates", "normal_impulse_ns",
    "kinetic_j", "elastic_j", "gravity_j", "finite_triangle_min_distance_m",
    "min_tetra_j",
)


def load_native(path: Path) -> list[dict[str, float]]:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        missing = set(FIELDS).difference(reader.fieldnames or ())
        if missing:
            raise ValueError(f"missing original native columns: {sorted(missing)}")
        rows = []
        for line, raw in enumerate(reader, start=2):
            try:
                r = {key: float(raw[key]) for key in FIELDS}
            except (ValueError, TypeError, KeyError) as exc:
                raise ValueError(f"invalid native row at CSV line {line}") from exc
            if not all(math.isfinite(v) for v in r.values()):
                raise ValueError(f"nonfinite native source row at CSV line {line}")
            for field in ("step", "accepted_leaves", "rejected_geom",
                          "rejected_tetra", "active_contact_pairs",
                          "face_impulse_updates"):
                if r[field] < 0 or not r[field].is_integer():
                    raise ValueError(f"invalid integer counter {field} at line {line}")
            if r["min_tetra_j"] <= 0 or r["finite_triangle_min_distance_m"] <= 0:
                raise ValueError(f"unsafe accepted physical state at line {line}")
            rows.append(r)
    if len(rows) < 2:
        raise ValueError("at least two original source states required")
    if rows[0]["step"] != 0:
        raise ValueError("source trajectory must begin at step zero")
    return rows


def analyze(rows: list[dict[str, float]], clock_tol: float = 2e-9):
    """Respect the initial zero-step receipt and each accepted physical interval."""
    measures = []
    by_regime = {
        "contact": {"steps": 0, "delta_energy_j": 0.0, "negative_changes_j": 0.0,
                    "positive_changes_j": 0.0},
        "no_contact": {"steps": 0, "delta_energy_j": 0.0, "negative_changes_j": 0.0,
                       "positive_changes_j": 0.0},
    }
    for previous, current in zip(rows, rows[1:]):
        i = int(current["step"])
        if i != int(previous["step"]) + 1:
            raise ValueError(f"non-contiguous physical source states before step {i}")
        dt = current["physical_time_s"] - previous["physical_time_s"]
        if dt <= 0 or abs(dt - current["actual_accepted_time_s"]) > clock_tol:
            raise ValueError(f"accepted elapsed-time ledger failed at step {i}")
        if current["accepted_leaves"] < 1:
            raise ValueError(f"no accepted physical substep at step {i}")
        k = current["kinetic_j"] - previous["kinetic_j"]
        u = current["elastic_j"] - previous["elastic_j"]
        g = current["gravity_j"] - previous["gravity_j"]
        d = k + u + g
        group = "contact" if current["face_impulse_updates"] > 0 else "no_contact"
        agg = by_regime[group]
        agg["steps"] += 1
        agg["delta_energy_j"] += d
        agg["negative_changes_j"] += min(0.0, d)
        agg["positive_changes_j"] += max(0.0, d)
        measures.append({
            "step": i, "time_s": current["physical_time_s"],
            "delta_kinetic_j": k, "delta_elastic_j": u,
            "delta_gravity_j": g, "delta_mechanical_j": d,
            "cumulative_delta_mechanical_j": (
                current["kinetic_j"] + current["elastic_j"] + current["gravity_j"]
                - rows[0]["kinetic_j"] - rows[0]["elastic_j"] - rows[0]["gravity_j"]
            ),
            "face_impulse_updates": int(current["face_impulse_updates"]),
            "accepted_leaves": int(current["accepted_leaves"]),
            "rejected_geom": int(current["rejected_geom"]),
            "rejected_tetra": int(current["rejected_tetra"]),
            "regime": group,
        })
    initial = sum(rows[0][f] for f in ("kinetic_j", "elastic_j", "gravity_j"))
    final = sum(rows[-1][f] for f in ("kinetic_j", "elastic_j", "gravity_j"))
    residual = sum(x["delta_mechanical_j"] for x in measures) - (final - initial)
    if abs(residual) > 1e-8 * max(1.0, abs(initial), abs(final)):
        raise ValueError("mechanical energy accounting failed telescoping identity")
    return measures, {
        "source_rows": len(rows),
        "physical_steps": len(measures),
        "elapsed_time_s": rows[-1]["physical_time_s"] - rows[0]["physical_time_s"],
        "initial_mechanical_j": initial,
        "final_mechanical_j": final,
        "delta_mechanical_j": final - initial,
        "telescoping_residual_j": residual,
        "minimum_face_separation_m": min(x["finite_triangle_min_distance_m"] for x in rows),
        "minimum_tetra_j": min(x["min_tetra_j"] for x in rows),
        "accepted_physical_leaves": sum(int(x["accepted_leaves"]) for x in rows[1:]),
        "rejected_geometry_trials": sum(int(x["rejected_geom"]) for x in rows[1:]),
        "rejected_tetra_trials": sum(int(x["rejected_tetra"]) for x in rows[1:]),
        "impulse_updates": sum(int(x["face_impulse_updates"]) for x in rows[1:]),
        "regimes": by_regime,
        "interpretation": (
            "Observed discrete mechanical-energy change only. "
            "Contact and no-contact labels do not isolate causes: gravity, "
            "damping, friction and numerical integration are coupled. "
            "No independent physical calibration or SCA-versus-IPC timing."
        ),
    }


def run() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True,
                        help="Original rigid_triangle_physics_900.csv")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = load_native(args.input)
    details, report = analyze(rows)
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "mechanical_energy_steps.csv").open(
        "w", encoding="utf-8", newline=""
    ) as stream:
        writer = csv.DictWriter(stream, fieldnames=list(details[0].keys()))
        writer.writeheader()
        writer.writerows(details)
    (args.output / "mechanical_energy_audit.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    run()
