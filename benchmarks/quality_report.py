#!/usr/bin/env python3
"""Coupled-solver qualitative and physical-quality metrics from real mesh JSONL.

A broad-phase that returns the exact same candidate pairs must NOT be credited
with magically lower fold-over or penetration when the physics solver is
unchanged. Quality outcomes are measured independently and policy-to-SAP
mesh parity is verified here at the per-vertex level.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from statistics import mean

def load(directory):
    result={}
    for file in sorted(Path(directory).glob("*.jsonl")):
        with file.open("r",encoding="utf-8") as stream:
            header=json.loads(stream.readline())
            if header.get("type")!="mesh_header":raise ValueError(f"Bad header {file}")
            data=[json.loads(line) for line in stream if line.strip()]
        if not data:raise ValueError(f"Missing frames in {file}")
        if any(row.get("type")!="frame" for row in data):
            raise ValueError(f"Nonframe payload in {file}")
        result[header["policy"]]=(header,data)
    if not result:raise ValueError("No solver mesh evidence")
    return result

def analyze(directory,tolerance=1e-7):
    inputs=load(directory)
    if "sap_rebuild" not in inputs:raise ValueError("Need SAP reference trajectory")
    source,ref=inputs["sap_rebuild"]
    records={}
    for name,(header,frames) in inputs.items():
        if header["triangles"]!=source["triangles"] or len(frames)!=len(ref):
            raise ValueError(f"{name}: inconsistent topology or frame count")
        divergence=[]
        for frame,baseline in zip(frames,ref):
            if frame["frame"]!=baseline["frame"]:
                raise ValueError(f"{name}: out-of-order frame")
            p=frame["positions"];q=baseline["positions"]
            if len(p)!=len(q):raise ValueError(f"{name}: vertex count mismatch")
            worst=max((math.dist(a,b) for a,b in zip(p,q)),default=0.)
            divergence.append(worst)
            if worst>tolerance:
                raise ValueError(f"{name}: altered solver geometry on frame {frame['frame']} by {worst:.7g}")
            m=frame["metrics"]
            if not all(math.isfinite(float(v)) for v in m.values() if isinstance(v,(float,int))):
                raise ValueError(f"{name}: nonfinite numeric physics diagnostic")
            if int(m["oracle_misses"])!=0 and m.get("oracle_checked",False):
                raise ValueError(f"{name}: missed audited broad-phase pairs")
        def avg(key):
            return mean(float(x["metrics"][key]) for x in frames)
        records[name]={
            "frames":len(frames),"max_vertex_distance_vs_SAP":max(divergence),
            "mean_step_ms_with_diagnostics":avg("step_ms"),
            "mean_broad_ms_with_diagnostics":avg("broad_ms"),
            "mean_narrow_ms_with_diagnostics":avg("narrow_ms"),
            "mean_rms_relative_edge_strain":avg("rms_relative_edge_strain"),
            "mean_near_degenerate_triangles":avg("near_degenerate_triangles"),
            "mean_self_crossing_response_events":avg("self_crossing_responses"),
            "mean_candidate_pairs":avg("candidate_pairs"),
            "mean_velocity_clamps":avg("velocity_clamps"),
        }
    return {
        "reference":"sap_rebuild",
        "all_trajectories_same_within_tolerance":True,
        "maximum_allowed_vertex_difference":tolerance,
        "results":records,
        "interpretation":"Geometry is equivalent across methods. The broad-phase did not improve cloth folding, penetration, or physics fidelity in this experiment.",
        "limitations":[
            "Area collapse proxy is not a full triangle inversion/coplanar intersection oracle",
            "self_crossing_response_events counts detected solver responses, not residual post-solve triangle intersections",
            "No trajectory compared with real garment motion unless an externally aligned reference is added",
            "With auditing enabled, measured step_ms includes diagnostics and is not unbiased solver timing",
            "Discrete AABB candidate equivalence is not continuous collision detection",
        ]
    }

def write_report(data,directory):
    directory=Path(directory)
    directory.mkdir(parents=True,exist_ok=True)
    (directory/"SOLVER_QUALITY.json").write_text(json.dumps(data,indent=2)+"\n")
    rows=["# Coupled-solver visual quality report","",
          "**All solver trajectories matched SAP within the declared vertex tolerance.**",
          "",
          "This is **not** evidence of a broad-phase reducing folds, because the geometry remained the same.","",
          "| Policy | Step ms* | Broad ms* | RMS edge strain | Near-degenerate triangles | Self-crossing responses | Max vertex error |",
          "|---|---:|---:|---:|---:|---:|---:|"]
    for k,v in data["results"].items():
        rows.append(f'| {k} | {v["mean_step_ms_with_diagnostics"]:.3f} | '
                    f'{v["mean_broad_ms_with_diagnostics"]:.3f} | '
                    f'{v["mean_rms_relative_edge_strain"]:.4f} | '
                    f'{v["mean_near_degenerate_triangles"]:.2f} | '
                    f'{v["mean_self_crossing_response_events"]:.2f} | '
                    f'{v["max_vertex_distance_vs_SAP"]:.2g} |')
    rows.extend(["","*Solver timings include enabled auditing; see the standalone timing benchmark for unbiased runtime.*",
                 "","## Measurement limits",""]+
                ["- "+line for line in data["limitations"]])
    (directory/"SOLVER_QUALITY.md").write_text("\n".join(rows)+"\n")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("mesh_dir",type=Path)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--tolerance",type=float,default=1e-7)
    args=p.parse_args()
    if not 0<=args.tolerance<=.01:raise SystemExit("unreasonable tolerance")
    report=analyze(args.mesh_dir,args.tolerance)
    write_report(report,args.output)
    print(args.output/"SOLVER_QUALITY.md")

if __name__=="__main__":main()
