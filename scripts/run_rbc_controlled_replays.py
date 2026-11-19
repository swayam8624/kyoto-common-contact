#!/usr/bin/env python3
"""Controlled, source-receipted 10-policy RBC vs Guard sensitivity experiment.

Accepts immutable ORIGINAL KYOTOM01 sources via either a prior Kyoto
HELDOUT_RESULTS.json or a native RBC_RESULTS.json. No source is generated,
moved, overwritten or re-simulated; this replays the same frame bytes
using new method instances for each repetition.

Each native replay executes 10 real complete-pair methods:
 Guard .045 A/A, Guard .10/.20, indexed RBC same 3 skins,
 rebuilt/persistent SAP and frozen V4. Methods run whole-trajectory
 forward/reverse schedule on alternating repetitions; this is NOT a
 randomized independent ABBA experiment across thermal sessions.

Source receipt SHA-256 is verified before AND after; per-frame pair
 hash and count must agree across all policies and repeats. Summary
 reports per-SCENE averaged full-pair ratio, not an invented GPU or
 solver full-step win; A/A control is reported as a noise diagnostic.

Never use inspected held-out sources for claiming an untouched test.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import platform
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path

METHODS = (
    "guard_045_A", "guard_045_B", "guard_100", "guard_200",
    "rbc_indexed_045", "rbc_indexed_100", "rbc_indexed_200",
    "sap_rebuild", "sap_persistent", "guard_sap_bounded_trial_frozen"
)

def digest(path: Path) -> str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(1<<20),b""):
            h.update(chunk)
    return h.hexdigest()

def positive(v: str) -> float:
    n=float(v)
    if not math.isfinite(n) or n<0:
        raise ValueError("Nonfinite or negative measured native query time")
    return n

def gm(xs: list[float]) -> float:
    if not xs or any(not math.isfinite(x) or x<=0 for x in xs):
        raise ValueError("Geomean needs positive finite independent ratios")
    return math.exp(statistics.mean(math.log(x) for x in xs))

def source_paths(path: Path) -> list[dict]:
    document=json.loads(path.read_text("utf-8"))
    if "material_replays" in document:
        return [
            {"material":r["material"],"source":r["snapshot"],
             "sha256":r["snapshot_sha256"]}
            for r in document["material_replays"]
        ]
    if "native_material_scenes" in document:
        return [
            {"material":r["material"],"source":r["source"],
             "sha256":r["source_sha256"]}
            for r in document["native_material_scenes"]
        ]
    raise ValueError("Not an original Kyoto heldout/RBC source manifest")

def inspect_native(path: Path,repeats: int,warmup: int) -> dict:
    with path.open(newline="",encoding="utf-8") as fd:
        rows=list(csv.DictReader(fd))
    if not rows:
        raise ValueError("Empty native output")
    n=int(rows[0]["n"])
    source_seed=int(rows[0]["source_seed"])
    frame_max=max(int(r["frame"]) for r in rows)
    expected_frames=list(range(warmup,frame_max+1))
    by_slot=defaultdict(dict)
    pairs=defaultdict(set)
    for row in rows:
        method=row["method"]
        repeat=int(row["repeat"])
        frame=int(row["frame"])
        if method not in METHODS or repeat<0 or repeat>=repeats:
            raise ValueError("Unexpected native method or repetition")
        if frame<warmup or frame>frame_max:
            raise ValueError("Unexpected warmup/frame")
        if int(row["n"])!=n or int(row["source_seed"])!=source_seed:
            raise ValueError("Mixed native source metadata")
        expected_slot=(METHODS.index(method) if repeat%2==0 else
                       len(METHODS)-1-METHODS.index(method))
        if int(row["slot"])!=expected_slot:
            raise ValueError("Method order schedule not reproducible")
        if row["oracle_equal"]!="1":
            raise ValueError("Original native all-frame SAP oracle failed")
        if int(row["pairs"])<0:
            raise ValueError("Negative candidate count")
        measured=positive(row["ms"])
        if int(row["v4_trial"]) and row["method"]!="guard_sap_bounded_trial_frozen":
            raise ValueError("Non-V4 native method reported V4 trial")
        key=(method,repeat)
        if frame in by_slot[key]:
            raise ValueError("Duplicate method/repeat/frame observation")
        by_slot[key][frame]=measured
        pairs[frame].add((row["pairs"],row["pair_hash"]))
    expected={(name,rep) for name in METHODS for rep in range(repeats)}
    if set(by_slot)!=expected:
        raise ValueError("Native full-policy cohort incomplete")
    if any(sorted(values)!=expected_frames for values in by_slot.values()):
        raise ValueError("Native source frames missing/non-contiguous")
    if any(len(v)!=1 for v in pairs.values()) or len(pairs)!=len(expected_frames):
        raise ValueError("Native original candidate pair-hash stream mismatch")
    per_repeat={m:[] for m in METHODS}
    for r in range(repeats):
        for m in METHODS:
            per_repeat[m].append(statistics.mean(by_slot[m,r].values()))
    means={name:statistics.mean(vals) for name,vals in per_repeat.items()}
    if means["guard_045_A"]<=0 or means["guard_045_B"]<=0:
        raise ValueError("A/A source Guard time must be positive")
    return {
        "n":n,"source_seed":source_seed,
        "timed_frame_count":len(expected_frames),
        "native_rows":len(rows),"all_current_full_pair_hashes_identical":True,
        "per_method_repeat_mean_ms":per_repeat,
        "per_method_mean_ms":means,
        "guardA_over_guardB":means["guard_045_A"]/means["guard_045_B"],
        "guardA_over_method":{
            name:(means["guard_045_A"]/avg if avg>0 else None)
            for name,avg in means.items()
        },
    }

def run(replayer: Path,manifest: Path, output: Path,
        repeats: int,warmup: int,limit: int) -> dict:
    if output.exists():
        raise FileExistsError("Refusing to overwrite existing RBC study: "+str(output))
    sources=source_paths(manifest)
    if limit:
        sources=sources[:limit]
    if not sources:
        raise ValueError("Original source manifest selects no scenes")
    selected=[]
    for i,s in enumerate(sources):
        path=Path(s["source"])
        if not path.is_file():
            raise FileNotFoundError("Original source not accessible: "+str(path))
        actual=digest(path)
        if actual!=s["sha256"]:
            raise ValueError("Source receipt SHA mismatch: "+str(path))
        selected.append((i,s,path,actual))
    output.mkdir(parents=True,exist_ok=False)
    records=[]
    for i,s,path,original_sha in selected:
        stem=f"scene-{i:03d}-{s['material']}-{original_sha[:12]}"
        native=output/(stem+".csv")
        log=output/(stem+".log")
        argv=[str(replayer),str(path),str(native),str(repeats),str(warmup)]
        process=subprocess.run(argv,capture_output=True,text=True,
                               timeout=1200,check=False)
        log.write_text("COMMAND: "+" ".join(argv)+"\n"+
                       process.stdout+process.stderr,encoding="utf-8")
        if process.returncode:
            raise RuntimeError("RBC native full-pair replay failed: "+str(log))
        if digest(path)!=original_sha:
            raise ValueError("Immutable source changed during native replay")
        report=inspect_native(native,repeats,warmup)
        records.append({
            "index":i,"material":s["material"],"source":str(path),
            "source_sha256":original_sha,"native_csv":str(native),
            "native_csv_sha256":digest(native),
            "native_log":str(log),"result":report,
        })
        print(f"PASS RBC matched source #{i+1}/{len(selected)}:"
              f" {s['material']} N={report['n']} methods={len(METHODS)}"
              f" A/A={report['guardA_over_guardB']:.4f}x",
              flush=True)
    scene_scores={}
    for method in METHODS:
        ratios=[r["result"]["guardA_over_method"][method] for r in records]
        scene_scores[method]={
            "guardA_over_method_geomean":gm(ratios),
            "scene_wins":sum(x>1 for x in ratios),
            "scene_count":len(ratios),
            "source_scene_ratios":ratios
        }
    outcome={
        "schema":"rbc-ten-policy-repeated-full-pair-study-v1",
        "experiment_type":"REUSE OF HISTORICAL INSPECTED MATERIAL SOURCE COHORT; NOT UNSEEN HOLDOUT",
        "manifest_path":str(manifest),
        "manifest_sha256":digest(manifest),
        "replayer_path":str(replayer),
        "replayer_sha256":digest(replayer),
        "repetitions":repeats,"warmup":warmup,
        "platform":platform.platform(),
        "source_scene_count":len(records),
        "method_names":list(METHODS),
        "all_original_source_sha256_receipts_verified":True,
        "all_natively_emitted_pair_sets_exact":True,
        "method_scores":scene_scores,
        "scenes":records,
        "cautions":[
            "This is a REUSED inspected source cohort, NOT an untouched generalization test",
            "Whole-trajectory method order alternates but has no independent thermal session calibration",
            "A/A noise floor is same-program Guard copy ratio, not a population significance interval",
            "All 10 algorithms emit FULL exact source candidate sets and charge bookkeeping and pair emission",
            "The source material snapshots are already padded and contain no real solver contact decisions",
            "Repeated timings on the same trajectory are not independent sample scenes",
            "No end-to-end solver speed or CCD/penetration correctness follows",
        ],
    }
    (output/"RBC_CONTROLLED_RESULTS.json").write_text(
        json.dumps(outcome,indent=2)+"\n",encoding="utf-8")
    aa=[r["result"]["guardA_over_guardB"] for r in records]
    lines=[
        "# RBC 10-policy repeated native full-pair comparison",
        "",
        "All original KYOTOM01 source SHA-256 receipts independently checked;",
        "no original input or historical policy is modified.",
        "",
        f"Scenes: {len(records)}; repetitions: {repeats}; warmup: {warmup}.",
        f"Guard A/B per-scene geomean: {gm(aa):.5f}x",
        f"; A/A maximum absolute ratio deviation: {max(abs(x-1) for x in aa):.4f}.",
        "",
        "| Native full-pair algorithm | Guard A/method geomean | Scene wins |",
        "|---|---:|---:|"
    ]
    for method in METHODS:
        score=scene_scores[method]
        lines.append(
            f"| {method} | {score['guardA_over_method_geomean']:.5f}x | "
            f"{score['scene_wins']}/{score['scene_count']} |")
    lines+=["","## Evidence limits",""]+[
        "- "+s for s in outcome["cautions"]]
    (output/"RBC_CONTROLLED_RESULTS.md").write_text(
        "\n".join(lines)+"\n",encoding="utf-8")
    return outcome

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--replayer",required=True,type=Path)
    p.add_argument("--manifest",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--repeats",type=int,default=4)
    p.add_argument("--warmup",type=int,default=8)
    p.add_argument("--limit",type=int,default=0)
    args=p.parse_args()
    if not args.replayer.is_file() or not args.manifest.is_file():
        p.error("Built native executable and original manifest must exist")
    if not 2<=args.repeats<=32 or args.warmup<0 or args.limit<0:
        p.error("Need at least two repetitions and nonnegative warmup/limit")
    result=run(args.replayer,args.manifest,args.output,
               args.repeats,args.warmup,args.limit)
    print("PASS: ten-policy independent receipts, native pair equality"
          " and Guard A/A control completed for",
          result["source_scene_count"],"original scenes")
    print(args.output/"RBC_CONTROLLED_RESULTS.md")

if __name__=="__main__":
    main()
