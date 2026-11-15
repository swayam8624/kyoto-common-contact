#!/usr/bin/env python3
"""Resource-bounded, isolated adversarial Kyoto broad-phase stress matrix.

A timeout/OOM is a CENSORED FAILURE, not evidence of any competing algorithm
being incorrect or the adaptive algorithm being asymptotically superior.
Every method runs in its own process on an identical seeded deformation
sequence, and every successful CSV is audited for exact current AABB pairs.
"""
from __future__ import annotations
import argparse
import csv
import datetime
import json
import os
from pathlib import Path
import platform
import random
import signal
import subprocess
import sys
import time

POLICIES=("sap_rebuild","global_guarded_exact","selective_guard_exact",
          "region_fixed","region_adaptive","region_hierarchical",
          "online_cost_aware")
MODES={
 "quick":{"sizes":(512,2048),"materials":("cloth","soft_ball","sponge"),
          "regimes":("baseline","rapid"),"seeds":(101,211),"frames":24},
 "stress":{"sizes":(2048,8192,16384),"materials":
           ("cloth","sponge","cushion","soft_ball","spring","mixed"),
           "regimes":("baseline","rapid"),"seeds":(101,211,307),"frames":48},
 "extreme":{"sizes":(16384,32768,65536,100000),"materials":
           ("cloth","sponge","cushion","soft_ball","spring","mixed"),
           "regimes":("rapid","compression"),"seeds":(101,211,307),"frames":40}
}

def rss_kb(pid):
    # macOS and Linux ps both accept -o rss=; RSS is not total system use,
    # and transient allocation spikes can be missed between polls.
    try:
        value=subprocess.check_output(["ps","-o","rss=","-p",str(pid)],
                                      text=True,stderr=subprocess.DEVNULL)
        return int(value.strip() or 0)
    except (ValueError,subprocess.SubprocessError):return 0

def execute(cmd,log,seconds,limit_mb):
    began=time.monotonic()
    peak=0;status="unknown";rc=None
    with log.open("w",encoding="utf-8") as output:
        proc=subprocess.Popen([str(c) for c in cmd],stdout=output,
                              stderr=subprocess.STDOUT,start_new_session=True)
        try:
            while proc.poll() is None:
                time.sleep(.10)
                peak=max(peak,rss_kb(proc.pid))
                if peak>limit_mb*1024:
                    status="memory_guard";break
                if time.monotonic()-began>seconds:
                    status="timeout";break
            if status in ("memory_guard","timeout"):
                try:os.killpg(proc.pid,signal.SIGTERM)
                except ProcessLookupError:pass
                try:proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    try:os.killpg(proc.pid,signal.SIGKILL)
                    except ProcessLookupError:pass
                    proc.wait()
            else:
                rc=proc.returncode
                status="success" if rc==0 else "algorithm_error"
        finally:
            if proc.poll() is None:
                proc.kill();proc.wait()
    return {"status":status,"returncode":rc,
            "wall_seconds":round(time.monotonic()-began,3),
            "max_sampled_rss_mb":round(peak/1024,2),
            "rss_sampling_limitation":"100 ms polling can miss very brief peaks"}

def verify(csvfile,method,frames):
    with csvfile.open(newline="",encoding="utf-8") as stream:
        rows=list(csv.DictReader(stream))
    if len(rows)!=frames or any(row["policy"]!=method for row in rows):
        raise ValueError("wrong or incomplete independent-policy CSV")
    framesSeen=[int(row["frame"]) for row in rows]
    if framesSeen!=list(range(frames)):raise ValueError("discontinuous frames")
    if any(int(row["misses"]) or int(row["extras"]) for row in rows):
        raise ValueError("failed independent AABB oracle")
    if sum(int(row["oracle_checked"]) for row in rows)==0:
        raise ValueError("no candidate oracle checks performed")
    if any(float(row["query_ms"])<0 for row in rows):
        raise ValueError("invalid broad-phase wall timings")
    return {"actual_primitives":int(rows[0]["primitives"]),
            "mean_query_ms":sum(float(r["query_ms"]) for r in rows)/len(rows),
            "max_pairs":max(int(r["pairs"]) for r in rows),
            "oracle_checks":sum(int(r["oracle_checked"]) for r in rows)}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode",choices=MODES,nargs="?",default="quick")
    parser.add_argument("--build",type=Path,default=Path("build/kyoto-material-macos"))
    parser.add_argument("--output",type=Path)
    parser.add_argument("--max-seconds-per-policy",type=int,default=240)
    parser.add_argument("--max-rss-mb",type=int,default=4096)
    parser.add_argument("--max-experiments",type=int,default=0,
                        help="Optional deliberate budget cap (0 means full matrix)")
    args=parser.parse_args()
    if args.max_seconds_per_policy<5 or args.max_rss_mb<256 or args.max_experiments<0:
        parser.error("invalid safety limits")
    root=Path(__file__).resolve().parents[1]
    os.chdir(root)
    when=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run=args.output or Path("output")/f"kyoto-stress-{when}-{args.mode}"
    if run.exists():parser.error("refusing to overwrite stress evidence")
    (run/"csv").mkdir(parents=True)
    (run/"logs").mkdir()
    binary=args.build.resolve()/"ccp_material_benchmark"
    if not binary.exists():raise SystemExit(f"Missing benchmark: {binary}; build native targets first")
    conf=MODES[args.mode]
    combinations=[(n,m,r,seed,policy)
        for n in conf["sizes"] for m in conf["materials"]
        for r in conf["regimes"] for seed in conf["seeds"]
        for policy in POLICIES]
    random.Random(1915).shuffle(combinations)
    if args.max_experiments:combinations=combinations[:args.max_experiments]
    manifest={"timestamp":when,"mode":args.mode,"configuration":conf,
              "hardware":platform.platform(),"machine":platform.machine(),
              "safety_limits":{"per_policy_seconds":args.max_seconds_per_policy,
                               "sampled_rss_mb":args.max_rss_mb},
              "scope":"isolated CPU broad-phase query, not full physics timestep",
              "results":[]}
    for idx,(size,mat,regime,seed,policy) in enumerate(combinations,1):
        label=f"{mat}-{regime}-n{size}-seed{seed}-{policy}"
        csvpath=run/"csv"/(label+".csv")
        print(f"[STRESS] {idx}/{len(combinations)} {label}",flush=True)
        commands=[binary,csvpath,mat,size,conf["frames"],seed,5,0,regime,policy]
        entry={"label":label,"target_elements":size,"material":mat,
               "regime":regime,"seed":seed,"policy":policy,
               **execute(commands,run/"logs"/(label+".log"),
                         args.max_seconds_per_policy,args.max_rss_mb)}
        if entry["status"]=="success":
            try:entry.update(verify(csvpath,policy,conf["frames"]))
            except Exception as exc:
                entry["status"]="invalid_output";entry["error"]=repr(exc)
        manifest["results"].append(entry)
        (run/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
        print(f"[STRESS] {entry['status']} "
              f"query_mean_ms={entry.get('mean_query_ms','NA')} "
              f"sampled_peak_RSS_MB={entry['max_sampled_rss_mb']}",flush=True)
    success=sum(r["status"]=="success" for r in manifest["results"])
    from collections import Counter
    counts=dict(Counter(r["status"] for r in manifest["results"]))
    summary=(
      f"# Resource-bounded Kyoto stress run {when}\n\n"
      f"Successful, exact-oracle-checked executions: {success}/{len(combinations)}.\n\n"
      f"Failure/timeout categories: `{counts}`.\n\n"
      "Do not interpret a competitor timeout as a correct asymptotic defeat. "
      "At extreme collision density, any algorithm enumerating exact pairs "
      "must pay at least the output-size complexity. Measurements are "
      "per-query CPU, not full-frame physics or GPU. RSS is sampled, not a "
      "precise operating system peak. Use manifest and raw CSV for analysis.\n"
    )
    (run/"STRESS_REPORT.md").write_text(summary)
    with Path("DEVLOG.md").open("a",encoding="utf-8") as handle:
        handle.write(f"\n### Safely censored stress experiment — {when}\n\n")
        handle.write(f"- Manifest: `{run}/manifest.json`; valid runs: "
                     f"{success}/{len(combinations)}; statuses: `{counts}`.\n")
        handle.write("- Every competitor is subject to identical per-process "
                     "timeout and sampled RSS caps; incomplete results are "
                     "censored, never counted as correct wins. "
                     "See logs for negative findings and input sizes.\n")
    print(summary)
    print(f"[STRESS] Raw CSV and failure logs: {run}")
    print("[STRESS] Bundle: python3 scripts/package_kyoto_results.py",run)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
