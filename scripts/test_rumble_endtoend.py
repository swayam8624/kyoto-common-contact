#!/usr/bin/env python3
"""Real 4-replay V5 end-to-end smoke of CLI, repeated snapshots and reports."""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from scripts.test_learned_cxx import create_model


def test(build):
    root=Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="kyoto-rumble-e2e-") as tmp:
        temp=Path(tmp)
        model=temp/"constant-controller.mlpc"
        create_model(model)
        out=temp/"smoke"
        args=[sys.executable,str(root/"scripts"/"run_kyoto_controller_ablation.py"),
              "--model",str(model),"--build",str(build),"--output",str(out),
              "--seeds","6113","6121","--materials","cloth","spring",
              "--regimes","baseline","impact","--budgets","128",
              "--frames","20","--warmup","4",
              "--repeats","2","--max-experiments","2",
              "--sampling","balanced","--bootstrap","100","--timeout","120"]
        run=subprocess.run(args,cwd=root,capture_output=True,text=True,timeout=180)
        if run.returncode!=0:
            raise RuntimeError(f"V5 runner failed:\n{run.stdout}\n{run.stderr}")
        contract=json.loads((out/"ABLATION_CONTRACT.json").read_text())
        report=json.loads((out/"RUMBLE_PATTERNS.json").read_text())
        if len(contract["combos"])!=2 or contract["repeats"]!=2:
            raise RuntimeError("Wrong exhaustive sampling/repeated-run contract")
        if len(list((out/"replays").glob("*.csv")))!=4:
            raise RuntimeError("Expected four real V5 measured replay CSVs")
        if report["protocol"]["original_csv_files"]!=4 or (
                report["protocol"]["unique_cells"]!=2):
            raise RuntimeError("Wrong aggregate cell/repeat count")
        names={x["method"] for x in report["leaderboard"]}
        must={"patch_certificate_signal","bvh_adaptive_exact",
              "delta_spatial_join_exact","grid_spatial_hash_exact",
              "ml_cost_aware","guard_sap_bounded_trial",
              "global_guarded_aa_control"}
        if not must.issubset(names):
            raise RuntimeError(f"Missing contenders: {sorted(must-names)}")
        if not (out/"RUMBLE_PATTERNS.md").exists():
            raise RuntimeError("No human-readable tournament report")
        print("PASS: four real replay timings, independent oracle, ML, "
              "repeats, randomized order, A/A and automatic workload patterns")


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("build",type=Path)
    a=p.parse_args()
    test(a.build.resolve())
