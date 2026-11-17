#!/usr/bin/env python3
"""Exactness-first, CPU broad-phase benchmark scorer for *one frozen replay*.

This is not a physics-fidelity scorer. Each policy must return the same
full sorted pair set as the independent SAP reference on EVERY frame.
Reports paired mean, P95 and speedups; no made-up confidence interval from
correlated frames or fabricated blanket superiority.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean

def p95(values):
    ordered=sorted(values)
    return ordered[math.ceil(.95*len(ordered))-1]

def score(path,skip=8):
    data=defaultdict(dict)
    with Path(path).open(newline="",encoding="utf-8") as fd:
        reader=csv.DictReader(fd)
        cols={"backend","frame","ms","pairs","pair_hash","misses"}
        if not cols.issubset(reader.fieldnames or []):
            raise ValueError(f"CSV missing required fields: {cols-set(reader.fieldnames or [])}")
        for row in reader:
            method=row["backend"];frame=int(row["frame"])
            if frame in data[method]:raise ValueError(f"duplicate {method} frame {frame}")
            time=float(row["ms"])
            if not math.isfinite(time) or time<=0:
                raise ValueError(f"{method} {frame}: invalid runtime")
            data[method][frame]={
                "ms":time,"pairs":int(row["pairs"]),
                "hash":row["pair_hash"],"misses":int(row["misses"])
            }
    ref=data.get("sap_rebuild")
    if not ref:raise ValueError("No exact SAP oracle reference present")
    frames=set(ref)
    if frames!=set(range(len(frames))):raise ValueError("Non-contiguous oracle frames")
    if skip<0 or skip>=len(frames):raise ValueError("Invalid warmup skip")
    for method,observed in data.items():
        if set(observed)!=frames:
            raise ValueError(f"{method} misses frames")
        for f in sorted(frames):
            if observed[f]["misses"] or (
                observed[f]["pairs"],observed[f]["hash"])!=(
                ref[f]["pairs"],ref[f]["hash"]):
                raise ValueError(f"INCORRECT {method} frame {f}: oracle pair mismatch")
    results={}
    for method,observed in sorted(data.items()):
        timings=[observed[f]["ms"] for f in range(skip,len(frames))]
        results[method]={
            "mean_ms":mean(timings),
            "p95_ms":p95(timings),
            "frames_measured":len(timings),
            "mean_pairs":mean(observed[f]["pairs"] for f in range(skip,len(frames))),
            "full_frame_pair_parity":True
        }
    baseline=results["global_guarded_exact"] if "global_guarded_exact" in results else results["sap_rebuild"]
    for r in results.values():
        r["speedup_vs_reference_mean"]=baseline["mean_ms"]/r["mean_ms"]
        r["speedup_vs_reference_p95"]=baseline["p95_ms"]/r["p95_ms"]
    return {
        "source_csv":str(path),"warmup_excluded":skip,
        "baseline":"global_guarded_exact" if "global_guarded_exact" in results else "sap_rebuild",
        "methods":results,
        "correctness":"ALL frames match independent replay SAP hash and pair counts",
        "limitations":[
            "One replay cannot establish confidence intervals or general workload superiority",
            "AABB candidate parity is not physical mesh penetration or contact response correctness",
            "Floating-point hash checks should be supplemented with independent brute-force oracle tests",
            "A replay has no end-to-end solver time or CCD evidence",
        ]
    }

def write_report(data,out):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    (out/"REPLAY_SCORES.json").write_text(json.dumps(data,indent=2)+"\n")
    lines=[
        "# One frozen scene: exact broad-phase results","",
        "**All policy pair counts/hashes agree with the SAP reference on every frame.**",
        "This verifies discrete snapshot candidate sets, not continuous-contact correctness.","",
        f'Baseline: **{data["baseline"]}**; warmup frames excluded: {data["warmup_excluded"]}.',"",
        "| Method | Mean ms | P95 ms | Mean speedup | P95 speedup |",
        "|---|---:|---:|---:|---:|"
    ]
    for method,r in sorted(data["methods"].items(),
                           key=lambda item:item[1]["mean_ms"]):
        lines.append(f'| {method} | {r["mean_ms"]:.5f} | {r["p95_ms"]:.5f} | '
                     f'{r["speedup_vs_reference_mean"]:.3f}× | '
                     f'{r["speedup_vs_reference_p95"]:.3f}× |')
    lines.extend(["","## Do not overclaim", ""]+["- "+v for v in data["limitations"]])
    (out/"REPLAY_SCORES.md").write_text("\n".join(lines)+"\n")
    return out/"REPLAY_SCORES.md"

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("csv",type=Path)
    p.add_argument("--warmup",type=int,default=8)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    print(write_report(score(a.csv,a.warmup),a.output))

if __name__=="__main__":main()
