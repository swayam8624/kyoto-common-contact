#!/usr/bin/env python3
"""Fresh-seed, full-frame, exact-pair Mac/native C++ controller ablation.

No PyTorch or model retraining. Never reuses the two previously inspected
held-out seeds 101/1009 by default. Samples predeclared configurations BEFORE
running and refuses to modify the original exhaustive run. Benchmarks every
compiled baseline on the same exported AABBs and reports actual switched ML
wall time, switch counts, and per-component overhead. No performance assertion.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import random
import subprocess
import time
from collections import Counter
from datetime import datetime, timezone
from statistics import mean

from evaluate_learned_replay import evaluate, render

MATERIALS=("cloth","sponge","cushion","soft_ball","spring","mixed")
REGIMES=("baseline","impact","shear","compression","rapid","relax")
BUDGETS=(128,512,2048)
DEFAULT_SEEDS=(1201,1301,1409,1511)
TIMES=("ml_feature_ms","ml_inference_ms","ml_decision_ms","ml_backend_ms",
       "ml_bookkeeping_ms","ml_wrapper_ms")


def checked(argv, log: Path, timeout: int) -> None:
    began=time.monotonic()
    with log.open("w",encoding="utf-8") as stream:
        try:
            result=subprocess.run([str(x) for x in argv],stdout=stream,
                                  stderr=subprocess.STDOUT,timeout=timeout,
                                  check=False)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"Timeout after {timeout}s; see {log}") from exc
    if result.returncode:
        raise RuntimeError(f"Exit {result.returncode}: {argv[0]}; see {log}")
    print(f"[ABLATION] {Path(str(argv[0])).name} took {time.monotonic()-began:.2f}s",flush=True)


def metrics(paths: list[Path], warmup: int) -> dict:
    totals=Counter()
    actions=Counter()
    switches=0
    frames=0
    bypassed=0
    selected_global=0
    for path in paths:
        with path.open(newline="",encoding="utf-8") as stream:
            records=csv.DictReader(stream)
            for row in records:
                if row["backend"]!="ml_cost_aware" or int(row["frame"])<warmup:
                    continue
                frames+=1
                actions[row["chosen_backend"]]+=1
                switches+=int(row["ml_switched"])
                bypassed+=int(row["ml_bypassed"])
                if row["chosen_backend"]=="global_guarded_exact":
                    selected_global+=1
                for field in TIMES:
                    if field not in row:
                        raise RuntimeError(f"{path}: missing new timing field {field}; rebuild replayer")
                    value=float(row[field])
                    if not math.isfinite(value) or value<0:
                        raise RuntimeError(f"{path}: invalid {field}: {value}")
                    totals[field]+=value
    if not frames:raise RuntimeError("No timed learned replay frames")
    return {"frames":frames,"switches":switches,
            "neural_bypass_frames":bypassed,
            "global_guard_selection_frames":selected_global,
            "selected_backends":dict(actions),
            "mean_component_ms_per_timed_frame":
                {k:totals[k]/frames for k in TIMES},
            "total_component_ms":dict(totals),
            "note":"Outer-wrapper time is measured C++ method wall-time minus internal "
                   "controller wall-time; no prior-run elapsed times were reused."}


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model",type=Path,required=True,
                        help="Original completed model/controller.mlpc")
    parser.add_argument("--build",type=Path,default=Path("build/kyoto-material-macos"))
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--seeds",nargs="+",type=int,default=list(DEFAULT_SEEDS))
    parser.add_argument("--materials",nargs="+",choices=MATERIALS,default=list(MATERIALS))
    parser.add_argument("--regimes",nargs="+",choices=REGIMES,default=list(REGIMES))
    parser.add_argument("--budgets",nargs="+",type=int,default=list(BUDGETS))
    parser.add_argument("--frames",type=int,default=100)
    parser.add_argument("--warmup",type=int,default=8)
    parser.add_argument("--max-experiments",type=int,default=36,
                        help="Deterministic shuffled pilot; 0 means complete Cartesian matrix")
    parser.add_argument("--sample-seed",type=int,default=20261117)
    parser.add_argument("--bootstrap",type=int,default=2000)
    parser.add_argument("--timeout",type=int,default=300)
    parser.add_argument("--resume",action="store_true",
                        help="Resume identical partial ablation, preserving finished CSVs")
    args=parser.parse_args()
    model=args.model.resolve()
    build=args.build.resolve()
    output=args.output.resolve()
    exporter=build/"ccp_material_export"
    replayer=build/"ccp_material_replay"
    if not model.is_file():parser.error(f"Missing trained model: {model}")
    if any(not p.is_file() for p in (exporter,replayer)):
        parser.error("Build ccp_material_export and ccp_material_replay first")
    if args.frames<=args.warmup+4 or args.warmup<0 or args.bootstrap<100:
        parser.error("Need frames > warmup + 4, warmup>=0, and bootstrap>=100")
    if any(s<0 for s in args.seeds) or len(set(args.seeds))!=len(args.seeds):
        parser.error("Seeds must be distinct nonnegative integers")
    if set(args.seeds)&{101,1009}:
        parser.error("Seeds 101 and 1009 were already inspected as test: choose fresh seeds")
    if any(b<1 or b>50000 for b in args.budgets):
        parser.error("Primitive budgets must be in 1..50000 (exporter may impose tighter limits)")
    if args.max_experiments<0 or args.timeout<10:
        parser.error("Invalid max experiments or timeout")
    combos=list(itertools.product(sorted(args.seeds),sorted(set(args.materials)),
                                  sorted(set(args.regimes)),sorted(set(args.budgets))))
    random.Random(args.sample_seed).shuffle(combos)
    if args.max_experiments:combos=combos[:args.max_experiments]
    if not combos:parser.error("Empty experiment matrix")
    contract={"model_sha256":hashlib.sha256(model.read_bytes()).hexdigest(),
              "source_model_path":str(model),"seeds":sorted(args.seeds),
              "materials":sorted(set(args.materials)),"regimes":sorted(set(args.regimes)),
              "budgets":sorted(set(args.budgets)),"frames":args.frames,
              "warmup":args.warmup,"max_experiments":args.max_experiments,
              "sample_seed":args.sample_seed,
              "combos":[list(x) for x in combos],
              "evidence":"New deterministic material snapshot experiments; no model tuning",
              "source_exhaustive_run_unchanged":True}
    manifest=output/"ABLATION_CONTRACT.json"
    if output.exists():
        if not args.resume or not manifest.is_file():
            parser.error("Output exists: choose a new folder or --resume")
        existing=json.loads(manifest.read_text(encoding="utf-8"))
        if existing!=contract:parser.error("Resume contract differs; refusing to mix experiments")
    else:
        if args.resume:parser.error("Cannot resume an output folder that does not exist")
        output.mkdir(parents=True)
        manifest.write_text(json.dumps(contract,indent=2)+"\n",encoding="utf-8")
    for folder in ("snapshots","replays","logs"):
        (output/folder).mkdir(exist_ok=True)
    paths=[]
    for i,(seed,material,regime,budget) in enumerate(combos,1):
        stem=f"actual-{material}-{regime}-n{budget}-seed{seed}"
        snapshot=output/"snapshots"/f"{stem}.kmcb"
        report=output/"replays"/f"{stem}-evaluated.csv"
        print(f"[ABLATION] {i}/{len(combos)}: {stem}",flush=True)
        if not snapshot.exists():
            tmp=snapshot.with_name(snapshot.name+".partial")
            try:
                checked([exporter,tmp,material,budget,args.frames,seed,regime],
                        output/"logs"/f"{stem}-export.log",args.timeout)
                os.replace(tmp,snapshot)
            finally:
                tmp.unlink(missing_ok=True)
        if not report.exists():
            tmp=report.with_name(report.name+".partial")
            try:
                checked([replayer,snapshot,tmp,0,model],
                        output/"logs"/f"{stem}-replay.log",args.timeout)
                os.replace(tmp,report)
            finally:
                tmp.unlink(missing_ok=True)
        paths.append(report)
    scenes,summary=evaluate(paths,args.warmup,args.bootstrap,args.sample_seed)
    # These are existing-implementation relative runtimes, not predicted policy labels.
    ratio_total=sum(s["measured_mean_ms"]["global_guarded_exact"] for s in scenes)/sum(
        s["measured_mean_ms"]["ml_cost_aware"] for s in scenes)
    summary["aggregate_total_scene_mean_guard_over_ml"]=ratio_total
    summary["model_sha256"]=contract["model_sha256"]
    summary["fresh_seeds"]=sorted(args.seeds)
    summary["interpretation"]=("Exploratory fresh-seed pilot, no retuning on these scenes; "
        "scene-bootstrap interval does not give seed-independent uncertainty.")
    analysis=output/"FRESH_SEED_RESULTS"
    analysis.with_suffix(".md").write_text(render(scenes,summary),encoding="utf-8")
    analysis.with_suffix(".json").write_text(
        json.dumps({"aggregate":summary,"scenes":scenes},indent=2)+"\n",
        encoding="utf-8")
    timing=metrics(paths,args.warmup)
    (output/"COMPONENT_TIMING.json").write_text(
        json.dumps(timing,indent=2)+"\n",encoding="utf-8")
    print("[ABLATION] COMPLETED: new output, original exhaustive data untouched",flush=True)
    print("[ABLATION] scenes:",len(scenes),"fresh seeds:",sorted(args.seeds),flush=True)
    print("[ABLATION] per-trajectory geo Guard/ML:",
          f'{summary["per_trajectory_paired_geometric_mean_ml_vs_global"]:.4f}x',flush=True)
    print("[ABLATION] sum of scene means Guard/ML:",f"{ratio_total:.4f}x",flush=True)
    print("[ABLATION] controller switches:",timing["switches"],flush=True)
    print("[ABLATION] full evidence:",output,flush=True)


if __name__=="__main__":
    main()
