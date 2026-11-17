#!/usr/bin/env python3
"""One reproducible Kyoto material + real solver visual evidence pipeline.

Default: six materials x six motion regimes x four broad-phase overlays,
plus four actual solver-policy mesh trajectories. No training. No 864-scene
tournament. Runs exactly the same frozen AABB snapshots for every backend.

This is a quick research *demonstration* and correctness smoke, not a new
independent performance study. Results are explicitly labelled as such.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
import platform
import subprocess
import sys
from pathlib import Path
from statistics import mean

MATERIALS=("cloth","spring","soft_ball","sponge","cushion","mixed")
REGIMES=("baseline","impact","shear","compression","rapid","relax")
SOLVER_POLICIES=("sap_rebuild","global_guarded","guard_sap_bounded_trial",
                 "online_contact_work")
VIEW_POLICIES=("sap_rebuild","global_guarded_exact","guard_sap_bounded_trial",
               "region_hierarchical")

def run(args):
    print("+ "+" ".join(str(a) for a in args),flush=True)
    subprocess.run([str(x) for x in args],check=True)

def checksum(path):
    hasher=hashlib.sha256()
    with Path(path).open("rb") as file:
        while True:
            chunk=file.read(1<<20)
            if not chunk:break
            hasher.update(chunk)
    return hasher.hexdigest()

def solver_records(path):
    with Path(path).open(encoding="utf-8") as file:
        rows=[json.loads(line) for line in file if line.strip()]
    if not rows or rows[0].get("type")!="header":
        raise ValueError(f"Invalid solver replay header: {path}")
    if any(x.get("type")!="frame" for x in rows[1:]):
        raise ValueError("Unexpected solver record")
    return rows[0],rows[1:]

def verify_solver(root):
    reference_header,reference=solver_records(root/"solver"/"sap_rebuild.jsonl")
    report={}
    for policy in SOLVER_POLICIES:
        header,frames=solver_records(root/"solver"/f"{policy}.jsonl")
        if header["triangles"]!=reference_header["triangles"] or (
                header["triangle_sheets"]!=reference_header["triangle_sheets"]):
            raise ValueError(f"Different mesh topology under {policy}")
        if len(frames)!=len(reference):
            raise ValueError(f"Different sequence lengths under {policy}")
        errors=[];steps=[];broad=[];collapsed=[];crossing=[]
        for k,(r,rr) in enumerate(zip(frames,reference)):
            if r["frame"]!=rr["frame"] or len(r["positions"])!=len(rr["positions"]):
                raise ValueError(f"Frame/topology mismatch for {policy}, frame {k}")
            if r["metrics"]["oracle_misses"]!=0:
                raise RuntimeError(f"Solver pair oracle failure for {policy} frame {k}")
            errors.append(max((math.dist(p,q) for p,q in
                               zip(r["positions"],rr["positions"])),default=0.))
            steps.append(float(r["metrics"]["step_ms"]))
            broad.append(float(r["metrics"]["broad_ms"]))
            collapsed.append(int(r["quality"]["degenerate_area_triangles"]))
            crossing.append(int(r["metrics"]["detected_self_crossings_during_solver"]))
        report[policy]={
            "mean_step_ms":mean(steps),
            "mean_broad_ms":mean(broad),
            "maximum_position_deviation_from_sap_m":max(errors),
            "max_area_collapse_triangles":max(collapsed),
            "sum_detected_crossing_events_during_solver":sum(crossing),
            "same_topology_and_initial_conditions":True,
            "frames":len(frames),
        }
    return report

def summarize_aabbs(root,regimes):
    result={}
    for regime in regimes:
        for material in MATERIALS:
            report=root/"materials"/regime/f"{material}.csv"
            by={}
            with report.open(newline="",encoding="utf-8") as f:
                for row in csv.DictReader(f):
                    method=row["backend"]
                    by.setdefault(method,[]).append(row)
            for method in VIEW_POLICIES:
                if method not in by:raise ValueError(f"Missing {method} for {material}/{regime}")
            frames={int(r["frame"]) for r in by["sap_rebuild"]}
            if not frames or frames!=set(range(len(frames))):
                raise ValueError(f"Broken AABB frame sequence: {material}/{regime}")
            oracle={int(r["frame"]):(r["pair_hash"],int(r["pairs"]))
                    for r in by["sap_rebuild"]}
            out={}
            for method in VIEW_POLICIES:
                rows=by[method]
                if {int(r["frame"]) for r in rows}!=frames:
                    raise ValueError(f"Missing backend frames for {method} {material}/{regime}")
                if any((r["pair_hash"],int(r["pairs"]))!=oracle[int(r["frame"])]
                       or int(r["misses"])!=0 for r in rows):
                    raise RuntimeError(f"FATAL candidate-pair oracle failure for {method} {material}/{regime}")
                out[method]={
                    "mean_query_ms":mean(float(r["ms"]) for r in rows),
                    "frame_count":len(rows),
                    "all_exact_pair_hashes_match":True,
                }
            result[f"{material}/{regime}"]=out
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=Path("output/kyoto-visual-evidence"))
    parser.add_argument("--build",type=Path,default=Path("build/kyoto-visual"))
    parser.add_argument("--frames",type=int,default=36)
    parser.add_argument("--solver-frames",type=int,default=48)
    parser.add_argument("--elements",type=int,default=128)
    parser.add_argument("--seed",type=int,default=6109)
    parser.add_argument("--regimes",nargs="*",choices=REGIMES,default=list(REGIMES))
    parser.add_argument("--skip-render",action="store_true")
    parser.add_argument("--resume",action="store_true")
    parser.add_argument("--render-frames",type=int,default=24)
    parser.add_argument("--solver-sheets",type=int,default=2)
    parser.add_argument("--solver-balls",type=int,default=2)
    args=parser.parse_args()
    if not(8<=args.frames<=240 and 8<=args.solver_frames<=240
           and 16<=args.elements<=4096 and 1<=args.seed<=2_000_000_000
           and 2<=args.render_frames<=120):
        parser.error("frames 8..240, elements 16..4096, render frames 2..120")
    root=Path(__file__).resolve().parent.parent
    target=(root/args.output).resolve();build=(root/args.build).resolve()
    if target.exists() and not args.resume:
        parser.error("Output exists; use a NEW directory or --resume")
    target.mkdir(parents=True,exist_ok=True)
    run(("cmake","-S",root,"-B",build,"-DCMAKE_BUILD_TYPE=Release","-DBUILD_TESTING=ON"))
    run(("cmake","--build",build,"--target","ccp_material_export",
         "ccp_material_replay","ccp_solver_visual_export",
         "ccp_final_broadphase_api_tests","--parallel","4"))
    run(("ctest","--test-dir",build,"--output-on-failure",
         "-R","final_broadphase_public_api_exact_pair_oracle"))
    exports=[]
    for regime in args.regimes:
        destination=target/"materials"/regime
        destination.mkdir(parents=True,exist_ok=True)
        for i,material in enumerate(MATERIALS):
            # Same snapshot policy for every algorithm, scene-specific seed
            # fixed BEFORE any measurement. This suite is demonstrative.
            seed=args.seed+100*i+11*REGIMES.index(regime)
            snapshot=destination/f"{material}.kmcb"
            csvout=destination/f"{material}.csv"
            if not snapshot.exists():
                run((build/"ccp_material_export",snapshot,material,args.elements,
                     args.frames,seed,regime))
            if not csvout.exists():
                run((build/"ccp_material_replay",snapshot,csvout,0))
            exports.append({"material":material,"regime":regime,
                            "seed":seed,"snapshot_sha256":checksum(snapshot),
                            "replay_sha256":checksum(csvout)})
    solver_dir=target/"solver"
    solver_dir.mkdir(parents=True,exist_ok=True)
    for policy in SOLVER_POLICIES:
        path=solver_dir/f"{policy}.jsonl"
        if not path.exists():
            run((build/"ccp_solver_visual_export",path,policy,
                 args.solver_frames,args.solver_sheets,args.solver_balls))
    score={
        "schema":"kyoto-visual-evidence-v1",
        "provenance":{"git_branch":"feature/kyoto-benchmark-visual-cleanup",
                      "platform":platform.platform(),"python":sys.version.split()[0],
                      "material_frames":args.frames,"solver_frames":args.solver_frames,
                      "elements_target":args.elements,"scene_seed_base":args.seed,
                      "explanation":"AABB scene visual comparisons share IDENTICAL frozen bounds. "
                                    "Solver renders use actual coupled physics, not the material proxy trajectories."},
        "snapshot_inputs":exports,
        "aabb_oracle":summarize_aabbs(target,args.regimes),
        "solver_geometry":verify_solver(target),
        "limitations":[
            "Not a statistically independent speed study.",
            "All 6 material AABB families are material generator snapshots, not 6 coupled cloth solvers.",
            "AABB false-positive pairs are not visual folding errors.",
            "Post-step degenerate area count is not a robust 3D triangle inversion proof.",
            "Detected self-crossings are solver events, not residual post-step intersection counts.",
            "No claim of sim-to-real accuracy without external benchmark trajectories.",
        ],
    }
    (target/"EVIDENCE_SUMMARY.json").write_text(json.dumps(score,indent=2)+"\n",encoding="utf-8")
    if not args.skip_render:
        run((sys.executable,root/"scripts"/"render_kyoto_visuals.py",
             "--root",target,"--mode","all","--frames",args.render_frames,
             "--regimes",*args.regimes))
    print(f"[COMPLETE] visual evidence and paired geometry verification: {target}")

if __name__=="__main__":
    main()
