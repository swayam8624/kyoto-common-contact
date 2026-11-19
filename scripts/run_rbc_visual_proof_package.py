#!/usr/bin/env python3
"""One command: REAL five-policy cloth physics -> independent proof -> 3D.

Everything is produced in ONE never-before-existing output directory:
- five separately advanced native solver CSV policy trajectories;
- original real per-frame mesh and obstacle JSONL for all five;
- independent per-frame and 7-sample-between-frame penetration report;
- properly depth-tested, infinitely looping GIF/PNG and source SHA receipt;
- self-contained interactive 3D HTML (with --offline), and SHA receipt;
- one auditable inventory of every native and rendered source hash.

This pipeline creates NO synthetic mesh, does not improve a paper
result by suppressing negative data, and refuses to overwrite evidence.
If the physical geometry fails strict penetration checks, it preserves
a FAIL report and DOES NOT render a misleading success visualization.

Requires Pillow/NumPy, original native ccp_rbc_coupled_tournament build.
--offline also requires locally installed pinned Three.js and esbuild.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import platform
import subprocess
from collections import defaultdict
from pathlib import Path

from audit_rbc_subframe_geometry import audit
from build_rbc_physics_viewer import make
from render_rbc_physics_studio import (
    POLICIES, file_sha256, render, source_data,
)


def require_native_csv(source:Path, frames:int, geo:int,topo:int)->dict:
    with source.open(newline="",encoding="utf-8") as f:
        rows=list(csv.DictReader(f))
    if len(rows)!=frames*5:
        raise ValueError("Native source policy coverage incomplete")
    observed=defaultdict(list)
    for row in rows:
        name=row["policy"]
        if name not in POLICIES:
            raise ValueError("Unexpected native physical policy")
        if row["scenario"]!="rounded_fold_relax" or \
                int(row["geometry_seed"])!=geo or \
                int(row["topology_seed"])!=topo:
            raise ValueError("Original solver source identity changed")
        if int(row["geometric_self_intersections"])!=0:
            raise ValueError("REAL cloth endpoint penetration recorded")
        if int(row["oracle_misses"])!=0:
            raise ValueError("Original broad-phase missed SAP candidates")
        if float(row["max_position_diff_sap"])>1e-7:
            raise ValueError("Physical state diverged from source SAP")
        if not math.isfinite(float(row["step_ms"])) or \
                not math.isfinite(float(row["broad_ms"])):
            raise ValueError("Nonfinite measured whole-step/query timings")
        observed[name].append(row)
    if set(observed)!=set(POLICIES):
        raise ValueError("Not all five native solver policies executed")
    if any([int(r["frame"]) for r in observed[m]]!=list(range(frames))
           for m in POLICIES):
        raise ValueError("Native solver policy frames missing/duplicate")
    return {
        m:{
            "native_frames":frames,
            "native_same_sheet_contact_corrections":sum(
                int(r["self_contacts"]) for r in observed[m]),
            "native_sphere_contact_corrections":sum(
                int(r["sphere_contacts"]) for r in observed[m]),
            "mean_whole_step_ms_after_first_eight":sum(
                float(r["step_ms"]) for r in observed[m][8:])/max(1,frames-8)
        } for m in POLICIES
    }


def execute(args)->dict:
    if args.output.exists():
        raise FileExistsError(
            "Research package output already exists; never overwrite original evidence")
    if not args.solver.is_file():
        raise FileNotFoundError("Build actual C++ solver first: "+str(args.solver))
    if args.frames<16 or args.frames>160:
        raise ValueError("Need 16..160 real solver frames")
    root=args.output
    root.mkdir(parents=True,exist_ok=False)
    native=root/"NATIVE_FIVE_POLICY_SOLVER.csv"
    mesh=root/"real-five-policy-mesh"
    log=root/"native-cpp.log"
    cmd=[str(args.solver.resolve()),str(native.resolve()),
         str(args.frames),"rounded_fold_relax",
         str(args.geometry_seed),str(args.topology_seed),"0",
         str(mesh.resolve())]
    process=subprocess.run(cmd,capture_output=True,text=True,
                           timeout=1800,check=False)
    log.write_text(
        "COMMAND "+" ".join(cmd)+"\n"+process.stdout+process.stderr,
        encoding="utf-8")
    if process.returncode:
        raise RuntimeError("Native physical solver failed; retain "+str(log))
    scores=require_native_csv(native,args.frames,
                              args.geometry_seed,args.topology_seed)
    source_audit=root/"INDEPENDENT_TRIANGLE_SUBFRAMES.json"
    evidence=audit(mesh,source_audit,args.subdivisions,require_zero=True)
    if evidence["source_endpoint_piercing_total"]!=0 or \
            evidence["source_linear_subframe_piercing_total"]!=0:
        raise ValueError("Independent source geometry did not pass")
    sources=source_data(mesh)
    gif=root/"ACTUAL_CLOTH_FIVE_POLICIES.gif"
    render_receipt=render(sources,gif,args.stride,args.fps)
    viewer=root/"INTERACTIVE_ORIGINAL_PHYSICS.html"
    viewer_receipt=make(mesh,viewer,offline=args.offline)
    if not viewer_receipt["all_original_endpoint_intersections_zero"]:
        raise ValueError("Original browser source is not physically clean")
    originals=[
        native,log,source_audit,gif,gif.with_suffix(".png"),
        gif.with_suffix(".json"),viewer,
        viewer.with_suffix(".viewer-receipts.json")
    ]+[sources[m][2] for m in POLICIES]
    manifest={
        "schema":"kyoto-rbc-end-to-end-physical-geometry-and-render-receipts-v1",
        "original_native_executable_SHA256":file_sha256(args.solver),
        "native_CSV_SHA256":file_sha256(native),
        "original_geometry_seed":args.geometry_seed,
        "original_topology_seed":args.topology_seed,
        "physical_source_scenario":"prepared rounded, zero-initial-overlap fold",
        "actual_source_solver_frames":args.frames,
        "independent_original_current_piercing_count":0,
        "independent_original_linear_sampled_piercing_count":0,
        "independent_linearly_sampled_states":args.frames*(args.subdivisions-1),
        "method_count":5,
        "method_statistics":scores,
        "render":render_receipt,
        "interactive":viewer_receipt,
        "original_artifacts_sha256":{
            str(p.relative_to(root)):file_sha256(p) for p in originals
        },
        "machine":platform.platform(),
        "limits":[
            "One computer/timing session, not a population significance result",
            "Preparing a rounded fold is not spontaneous physical folding",
            "Strict endpoint/linearly sampled triangle piercings not full CCD",
            "Original solver remains an experimental XPBD-style system, not calibrated FEM",
        ]
    }
    (root/"PHYSICAL_EVIDENCE_MANIFEST.json").write_text(
        json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print("PASS complete native Kyoto physical/visual proof package:",
          root,flush=True)
    print("Interactive original source:",viewer,flush=True)
    print("Looping real-geometry GIF:",gif,flush=True)
    return manifest


def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--solver",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--frames",type=int,default=80)
    p.add_argument("--geometry-seed",type=int,default=73013)
    p.add_argument("--topology-seed",type=int,default=86011)
    p.add_argument("--subdivisions",type=int,default=8)
    p.add_argument("--stride",type=int,default=3)
    p.add_argument("--fps",type=int,default=12)
    p.add_argument("--offline",action="store_true",
                   help="Embed pinned Three.js for a single-file offline 3D viewer")
    args=p.parse_args()
    if args.geometry_seed<1 or args.topology_seed<1:
        p.error("Need positive deterministic physical source seeds")
    if not 2<=args.subdivisions<=32 or not 1<=args.stride<=20:
        p.error("Invalid geometric substeps or frame stride")
    if not 1<=args.fps<=30:
        p.error("Invalid rendered playback speed")
    execute(args)


if __name__=="__main__":
    main()
