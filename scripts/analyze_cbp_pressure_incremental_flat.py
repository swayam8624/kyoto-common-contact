#!/usr/bin/env python3
"""Audit Phase 14 exact occupancy persistence vs separately timed full rebuild.

Every completed manager frame must match the complete pair-count and 64/32-bit
digests of each available, completed frozen global-grid or X-sweep row.
Capped frames are not valid performance controls or correctness evidence.
No multiple comparison/exploration is hidden in the persistent query time.
"""
from __future__ import annotations
import argparse
import csv
import math
from pathlib import Path
import statistics

FIELDS=("exact_pairs","hash64_sum","hash64_xor","hash32_sum","hash32_xor")
STAGES=("preflight_ms","index_maintenance_ms","decision_ms","query_ms",
        "complete_query_ms","generate_ms")


def rows(filename: Path) -> list[dict]:
    with filename.open(newline="",encoding="utf-8") as f:
        r=list(csv.DictReader(f))
    if not r:
        raise ValueError("Empty evidence "+str(filename))
    return r


def mean(values: list[float]):
    if not values:
        return "—", "—"
    if not all(math.isfinite(v) and v>=0 for v in values):
        raise ValueError("Nonfinite measured time")
    ordered=sorted(values)
    return f"{statistics.fmean(values):.4f}", f"{ordered[math.ceil(.95*len(ordered))-1]:.4f}"


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("persistent",type=Path)
    p.add_argument("frozen",type=Path)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():
        raise FileExistsError("Will not overwrite earlier benchmark analysis")
    latest=rows(a.persistent)
    control=rows(a.frozen)
    if len(control)!=2*len(latest):
        raise ValueError("Measured frame/CPU reference counts differ")
    n=int(latest[0]["n"])
    name=latest[0]["scene"]
    methods={"persistent_flat_3d_grid":0,"projected_x_sweep":0}
    valid_grid=[]
    valid_sweep=[]
    incomplete=0
    totalDirty=0
    rebuilds=0
    for f,r in enumerate(latest):
        if (int(r["frame"])!=f or int(r["n"])!=n or r["scene"]!=name):
            raise ValueError("Out-of-order or mismatched immutable frame")
        if r["status"]!="complete":
            incomplete+=1
            continue
        methods[r["selected_backend"]]+=1
        rebuilds+=int(r["rebuilt"])
        totalDirty+=int(r["changed_cell_ranges"])
        group=control[2*f:2*f+2]
        if [x["method"] for x in group]!=[
            "global_3d_hash_grid","global_x_sweep"
        ] or any(int(x["frame"])!=f for x in group):
            raise ValueError("Misaligned independent fixed CPU evidence")
        for original in group:
            if original["status"]!="complete":
                continue
            for key in FIELDS:
                if original[key]!=r[key]:
                    raise ValueError(f"Complete output mismatch frame {f}, {key}")
        if group[0]["status"]=="complete":
            valid_grid.append(float(group[0]["grid_build_ms"])+
                              float(group[0]["query_ms"]))
        if group[1]["status"]=="complete":
            valid_sweep.append(float(group[1]["query_ms"]))
    complete=[r for r in latest if r["status"]=="complete"]
    if not complete:
        raise ValueError("No exact completed manager frames")
    full=[float(r["complete_query_ms"]) for r in complete]
    fixed_grid_mean=(statistics.fmean(valid_grid) if valid_grid else None)
    persistent_mean=statistics.fmean(full)
    speedup=(fixed_grid_mean/persistent_mean if fixed_grid_mean else None)
    notes=[
        f"# Flat persistent spatial index vs frozen rebuild: {name}, N={n:,}",
        "",
        f"- Completed exact frames: **{len(complete)}/{len(latest)}**",
        f"- Incomplete/budget-capped frames: **{incomplete}**",
        f"- Full spatial rebuilds: **{rebuilds}**",
        f"- Objects whose occupied cell ranges required maintenance: **{totalDirty:,}**",
        f"- Actually executed persistent grid queries: **{methods['persistent_3d_grid']}**",
        f"- Actually executed dense X-sweeps: **{methods['projected_x_sweep']}**",
        f"- Frozen grid exact completed frames: **{len(valid_grid)}/{len(latest)}**",
        f"- Frozen sweep exact completed frames: **{len(valid_sweep)}/{len(latest)}**",
        "",
        "| Stage or control | Mean ms | P95 ms |",
        "|---|---:|---:|",
    ]
    for field in STAGES:
        mn,p95=mean([float(r[field]) for r in complete])
        notes.append(f"| {field} | {mn} | {p95} |")
    for key,values in (
        ("frozen_grid_complete_ms",valid_grid),
        ("frozen_sweep_query_ms",valid_sweep)
    ):
        mn,p95=mean(values)
        notes.append(f"| {key} | {mn} | {p95} |")
    notes+=["",
            f"**Measured fixed-grid / persistent whole-query ratio:** "
            f"{speedup:.3f}×" if speedup else "No full fixed-grid comparator.",
            "",
            "Interpretation: The persistent result includes per-frame range "
            "validation, exact cell membership repair/rebuild, controller decision "
            "and one selected candidate query. Inputs and timed processes are "
            "separate; this is exploratory performance, not statistical significance.",
            "",
            "The adapter's input world geometry is synthetic Float32 AABBs. "
            "It reports only count/fingerprints, not a full pair output buffer, "
            "triangle CCD, contact constraints or deformation quality.",
            "Do not compare the old Metal grid GPU kernel-only times to "
            "persistent complete CPU query times as if both were end-to-end.",
            "Sparse/unchanged cell-range fast paths are conventional incremental "
            "spatial-index design, not a novel SIGGRAPH technique.",
            ""]
    report="\n".join(notes)
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(report,encoding="utf-8")
    print(report)
    return 0


if __name__=="__main__":
    raise SystemExit(main())
