#!/usr/bin/env python3
"""Independent native-source paired whole-solver BVH audit.

The original C++23 tests already compare EVERY FEM node position/velocity,
actual time, impulse update, CCD/narrowphase count and adaptive leaf.
This additional audit validates the original measured wall-clock receipts
and reports the *complete solver* timing ratio without fabricating data.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
import statistics
from pathlib import Path

COLUMNS=("repeat","method","elapsed_wall_ns","accepted_leaves",
    "rejected_geom","rejected_tetra","actual_integrated_dt_s",
    "face_impulse_updates","full_mesh_CCD_narrowphase_pairs",
    "triangle_box_tests","BVH_node_tests",
    "minimum_real_triangle_gap_m","min_tetra_j")

def audit(path: Path) -> dict:
    with path.open(newline="",encoding="utf-8") as stream:
        reader=csv.DictReader(stream)
        if set(COLUMNS).difference(reader.fieldnames or ()):
            raise ValueError("incomplete original native solver source columns")
        rows=list(reader)
    if len(rows)!=8:
        raise ValueError(f"expected 8 native paired receipts, found {len(rows)}")
    groups={}
    for row in rows:
        if not all(key in row and row[key] is not None for key in COLUMNS):
            raise ValueError("truncated C++23 solver receipt")
        rep=int(row["repeat"])
        method=row["method"]
        if rep not in range(4) or method not in (
            "original_complete_FEM","opt_in_static_BVH_complete_FEM"):
            raise ValueError("unrecognized source repeat or native method")
        key=(rep,method)
        if key in groups:
            raise ValueError("duplicate native measurement")
        for k in COLUMNS:
            if k in ("repeat","method"):continue
            value=float(row[k])
            if not math.isfinite(value):
                raise ValueError(f"nonfinite original FEM source {k}")
            row[k]=value
        if row["elapsed_wall_ns"]<=0:
            raise ValueError("invalid native monotonic clock receipt")
        if row["minimum_real_triangle_gap_m"]<=0 or row["min_tetra_j"]<=0:
            raise ValueError("unsafe accepted source FEM state")
        groups[key]=row
    times_original=[]
    times_bvh=[]
    for rep in range(4):
        before=groups[(rep,"original_complete_FEM")]
        after=groups[(rep,"opt_in_static_BVH_complete_FEM")]
        for field in ("accepted_leaves","rejected_geom","rejected_tetra",
                      "actual_integrated_dt_s","face_impulse_updates",
                      "full_mesh_CCD_narrowphase_pairs",
                      "minimum_real_triangle_gap_m","min_tetra_j"):
            if not math.isclose(before[field],after[field],
                                rel_tol=1e-11,abs_tol=1e-12):
                raise ValueError(f"native physics observable differs: {field}")
        if before["accepted_leaves"]<=1 or before["rejected_geom"]<=0:
            raise ValueError("native physical adaptive collision not exercised")
        if before["BVH_node_tests"]!=0 or after["BVH_node_tests"]<=0:
            raise ValueError("native BVH was not correctly billed")
        if not before["triangle_box_tests"]>after["triangle_box_tests"]:
            raise ValueError("native BVH did not prune original triangle box tests")
        if not math.isclose(before["actual_integrated_dt_s"],.00075,abs_tol=1e-12):
            raise ValueError("0.75ms source adaptive physical time was lost")
        times_original.append(before["elapsed_wall_ns"])
        times_bvh.append(after["elapsed_wall_ns"])
    ratio=sum(times_original)/sum(times_bvh)
    return {
        "scope":"actual full C++23 neo-Hookean 20m/s adaptive impact solver step; same original physics",
        "repeats":4,
        "original_wall_time_total_ns":sum(times_original),
        "opt_in_rigid_BVH_wall_time_total_ns":sum(times_bvh),
        "ratio_of_totals_original_over_bvh":ratio,
        "median_paired_original_over_bvh":statistics.median(
            a/b for a,b in zip(times_original,times_bvh)),
        "original_wall_time_ns":times_original,
        "accelerated_wall_time_ns":times_bvh,
        "source_physical_step_s":.00075,
        "source_adaptive_leaves":int(groups[(0,"original_complete_FEM")]["accepted_leaves"]),
        "source_geometry_rejections":int(groups[(0,"original_complete_FEM")]["rejected_geom"]),
        "source_FEM_impulse_updates":int(groups[(0,"original_complete_FEM")]["face_impulse_updates"]),
        "source_triangle_box_tests_original":int(groups[(0,"original_complete_FEM")]["triangle_box_tests"]),
        "source_triangle_box_tests_bvh":int(groups[(0,"opt_in_static_BVH_complete_FEM")]["triangle_box_tests"]),
        "source_BVH_node_tests":int(groups[(0,"opt_in_static_BVH_complete_FEM")]["BVH_node_tests"]),
        "warning":"One deterministic physical impact, four paired runs; no independent physical solver, no statistical significance, no general speedup claim."
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    result=audit(a.input)
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"native_full_solver_bvh_audit.json").write_text(
        json.dumps(result,sort_keys=True,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,sort_keys=True,indent=2))

if __name__=="__main__":
    main()
