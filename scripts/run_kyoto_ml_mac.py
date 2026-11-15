#!/usr/bin/env python3
"""Reproducible Mac PyTorch + native C++ train/validate/held-out experiment.

This runner is FOREGROUND and resource-bounded: it creates independent
timestamps, gathers real teacher costs on the user's hardware, runs a
deterministic grid with validation early stopping, then measures a truly
switched learned controller on held-out source seeds. No synthetic fabricated
speedups or automatic deployment to the original coupled cloth solver.
"""
from __future__ import annotations
import argparse
import atexit
import datetime
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
import threading

MATERIALS=("cloth","sponge","cushion","soft_ball","spring","mixed")
MODES={
  "smoke":{"seeds":(101,211,307,401,503),"regimes":("baseline",),
           "budgets":(64,512),"frames":12,"trials":1,"search_epochs":12},
  "quick":{"seeds":(101,211,307,401,503),"regimes":("baseline","impact"),
           "budgets":(256,512),"frames":48,"trials":12,"search_epochs":160},
  "standard":{"seeds":(101,211,307,401,503,607,701,809),
              "regimes":("baseline","impact","shear","compression"),
              "budgets":(256,1024),"frames":72,"trials":48,"search_epochs":300},
  "exhaustive":{"seeds":(101,211,307,401,503,607,701,809,907,1009),
                "regimes":("baseline","impact","shear","compression","rapid","relax"),
                "budgets":(128,512,2048),"frames":100,"trials":10**6,
                "search_epochs":500},
}

def call(argv,logpath,cwd,timeout,quiet=False):
    print("[KYOTO-ML] EXEC:", " ".join(map(str,argv)),flush=True)
    started=time.monotonic()
    with logpath.open("w",encoding="utf-8") as log:
        proc=subprocess.Popen([str(a) for a in argv],cwd=cwd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT,text=True,
                              errors="replace",bufsize=1)
        exceeded={"value":False}
        def abort_overlong():
            exceeded["value"]=True
            proc.kill()
        timer=threading.Timer(timeout,abort_overlong)
        timer.daemon=True
        timer.start()
        try:
            # Buffered stdout remains in the log even if the tool fails.
            # Forward informational progress only, but preserve all output.
            for line in proc.stdout:
                log.write(line)
                if not quiet or ("[RESULT]" in line or "[FAIL]" in line or
                                 "[PASS]" in line or "[TRAIN]" in line or
                                 "[HELDOUT]" in line):
                    print(line.rstrip(),flush=True)
            status=proc.wait(timeout=15)
        except BaseException:
            proc.kill();proc.wait()
            raise
        finally:
            timer.cancel()
    if exceeded["value"]:
        raise TimeoutError(f"Command exceeded {timeout} s; inspect {logpath}")
    if status!=0:raise RuntimeError(f"Command failed ({status}); inspect {logpath}")
    return time.monotonic()-started

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode",choices=MODES,default="quick",nargs="?")
    parser.add_argument("--build",type=Path,default=Path("build/kyoto-material-macos"))
    parser.add_argument("--max-trials",type=int)
    parser.add_argument("--timeout",type=int,default=900)
    parser.add_argument("--device",choices=("cpu","mps","auto"),default="auto")
    parser.add_argument("--resume-cpu-data",type=Path,
                        help="Use pre-generated complete teacher CSV and snapshot directory")
    args=parser.parse_args()
    if args.timeout<30:parser.error("timeout must be at least 30 seconds per command")
    root=Path(__file__).resolve().parents[1]
    os.chdir(root)
    mode=MODES[args.mode]
    utc=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run=Path("output")/f"kyoto-learned-{utc}-{args.mode}"
    if run.exists():parser.error("refusing to overwrite existing research experiment")
    run.mkdir(parents=True)
    done={"success":False}
    def preserve():
        subprocess.run([sys.executable,"scripts/package_kyoto_results.py",
                        str(run),"--exit-code",0 if done["success"] else 1],
                       cwd=root,check=False)
    atexit.register(preserve)
    logs=run/"logs";logs.mkdir()
    teacher=run/"teacher";teacher.mkdir()
    snapshots=run/"snapshots";snapshots.mkdir()
    holdout=run/"heldout";holdout.mkdir()
    try:
        import torch
        print("[KYOTO-ML] PyTorch:",torch.__version__,
              "MPS:",torch.backends.mps.is_available(),flush=True)
    except ImportError as err:
        raise SystemExit("Missing PyTorch: python3 -m pip install torch numpy") from err
    metadata={"mode":args.mode,"host":platform.platform(),
              "python":sys.version,"pytorch":torch.__version__,
              "machine":platform.machine(),"cpu_count":os.cpu_count(),
              "source_sha":subprocess.check_output(
                  ["git","rev-parse","HEAD"],text=True).strip(),
              "configuration":mode}
    (run/"environment.json").write_text(
        json.dumps(metadata,indent=2)+"\n",encoding="utf-8")
    build=args.build.resolve()
    exporter=build/"ccp_material_export"
    replayer=build/"ccp_material_replay"
    homebrew=Path("/opt/homebrew/opt/cmake/bin/cmake")
    cmake=str(homebrew if homebrew.is_file() else "cmake")
    call([cmake,"--build",build,"--target",
          "ccp_material_export","ccp_material_replay","--parallel","4"],
         logs/"native-ml-build.log",root,args.timeout,quiet=False)
    for executable in (exporter,replayer):
        if not executable.is_file():
            raise SystemExit(f"Missing native executable: {executable}. "
                             "Run the updated Kyoto material smoke script first.")
    # User-supplied complete runs can be evaluated without redundant geometry
    # generation. Otherwise generate unique independently seeded trajectories.
    if args.resume_cpu_data:
        source=args.resume_cpu_data.resolve()
        if not source.is_dir():raise SystemExit("resume directory unavailable")
        teacher=source/"teacher";snapshots=source/"snapshots"
        if not teacher.is_dir() or not snapshots.is_dir():
            raise SystemExit("resume needs teacher/ and snapshots/ directories")
    else:
        combinations=[(seed,material,regime,budget)
                      for seed in mode["seeds"]
                      for material in MATERIALS
                      for regime in mode["regimes"]
                      for budget in mode["budgets"]]
        print(f"[KYOTO-ML] Collecting {len(combinations)} independently executed "
              "AABB snapshot/replay experiments",flush=True)
        for number,(seed,material,regime,budget) in enumerate(combinations,1):
            stem=f"actual-{material}-{regime}-n{budget}-seed{seed}"
            snapshot=snapshots/(stem+".kmcb")
            evidence=teacher/(stem+"-cpu.csv")
            print(f"[KYOTO-ML] DATA {number}/{len(combinations)}: {stem}",flush=True)
            call([exporter,snapshot,material,budget,mode["frames"],seed,regime],
                 logs/(stem+"-export.log"),root,args.timeout,quiet=True)
            call([replayer,snapshot,evidence,0],
                 logs/(stem+"-teacher.log"),root,args.timeout,quiet=True)
    trials=args.max_trials if args.max_trials is not None else mode["trials"]
    model_dir=run/"model"
    command=[sys.executable,"scripts/train_online_controller.py",teacher,
             "--output",model_dir,"--split","seed","--search","grid",
             "--search-epochs",mode["search_epochs"],
             "--final-epochs",mode["search_epochs"],
             "--patience",max(5,int(mode["search_epochs"]*.12)),
             "--device",args.device,"--seed",20261116,"--verbose"]
    if args.mode=="exhaustive":command+=["--exhaustive"]
    else:command+=["--max-trials",trials]
    # The exhaustive 156-architecture/500-epoch search is an intentionally
    # longer foreground job; never kill it after the ordinary 15-minute
    # per-trajectory watchdog. Per-trial early stopping remains mandatory.
    training_limit=max(args.timeout,86400 if args.mode=="exhaustive" else
                       14400 if args.mode=="standard" else 3600)
    call(command,logs/"torch-training.log",root,training_limit,quiet=False)
    manifest=json.loads((model_dir/"manifest.json").read_text())
    heldout_seeds=set(manifest["groups"]["test"])
    evidence=[]
    for source in sorted(teacher.glob("*-cpu.csv")):
        # Metadata is read from CSV instead of a fragile filename heuristic.
        with source.open(encoding="utf-8") as handle:
            from csv import DictReader
            row=next(DictReader(handle))
        if row["seed"] not in heldout_seeds:continue
        snapshot=snapshots/source.name.replace("-cpu.csv",".kmcb")
        output=holdout/source.name.replace("-cpu.csv","-evaluated.csv")
        call([replayer,snapshot,output,0,model_dir/"controller.mlpc"],
             logs/(output.stem+".log"),root,args.timeout,quiet=True)
        evidence.append(output)
    if not evidence:raise RuntimeError("No heldout actual C++ replay experiments")
    report=run/"HELDOUT_ACTUAL_RUNTIME.md"
    call([sys.executable,"scripts/evaluate_learned_replay.py",*evidence,
          "--output",report,"--warmup",min(8,mode["frames"]//4)],
         logs/"heldout-evaluation.log",root,args.timeout,quiet=False)
    evaluation=json.loads(report.with_suffix(".json").read_text())
    with Path("DEVLOG.md").open("a",encoding="utf-8") as handle:
        h=evaluation["aggregate"]
        handle.write(f"\n### Actual C++ learned controller heldout — {utc}\n\n")
        handle.write(f"- Source commit `{metadata['source_sha']}`; run `{run}`; "
                     f"seeds in heldout={sorted(heldout_seeds)}.\n")
        handle.write(f"- Deployed ML policy vs Global Guard: "
                     f"{h['per_trajectory_paired_geometric_mean_ml_vs_global']:.4f}x "
                     f"(trajectory-bootstrap 95% CI "
                     f"{h['bootstrap_trajectory_95pct_ci']}); "
                     f"win/loss scenes "
                     f"{h['ml_trajectories_faster_than_global']}/"
                     f"{h['ml_trajectories_slower_than_global']}.\n")
        handle.write("- No software deployment decision until full coupled "
                     "simulation and additional independent machines/physics "
                     "regimes are audited. Raw evidence, every trial and "
                     f"negative results are in `{run}`.\n")
    print(f"[KYOTO-ML] SUCCESS: all real holdout results saved under {run}")
    done["success"]=True
    return run

if __name__=="__main__":
    try:
        target=main()
    except BaseException as exc:
        print("[KYOTO-ML] FAILURE:",repr(exc),file=sys.stderr,flush=True)
        raise
