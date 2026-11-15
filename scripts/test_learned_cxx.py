#!/usr/bin/env python3
"""C++ neural-policy ABI/correctness regression, no PyTorch dependency.

Construct a deterministic valid constant-cost model, replay known deforming
material snapshots, verify full exact pair hashes across all methods, and
prove that malformed model files are rejected instead of silently falling
back to unsafe or unspecified behavior.
"""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile

def run(*cmd,success=True):
    result=subprocess.run([str(c) for c in cmd],capture_output=True,text=True,
                          timeout=90,check=False)
    if (result.returncode==0)!=success:
        raise RuntimeError(f"Unexpected exit code for {cmd}\n"
                           f"stdout: {result.stdout}\nstderr: {result.stderr}")

def create_model(path):
    # depth=0 means a single Linear(12,4), all weights are zero. The bias
    # makes Global Guard the cheapest on every frame.
    body=["KYOTO_MLP_V1","12 1 0 4",
          " ".join(["0"]*12)," ".join(["1"]*12),
          " ".join(["0"]*(12*4)),
          "0 -3 -2 -1"]
    path.write_text("\n".join(body)+"\n",encoding="utf-8")

def main():
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument("exporter",type=Path)
    arg.add_argument("replayer",type=Path)
    args=arg.parse_args()
    with tempfile.TemporaryDirectory(prefix="kyoto-learned-smoke-") as tmp:
        root=Path(tmp)
        model=root/"constant.mlpc"
        create_model(model)
        samples=root/"cloth.kmcb"
        report=root/"cpu.csv"
        run(args.exporter,samples,"cloth",512,12,4321,"impact")
        run(args.replayer,samples,report,0,model)
        with report.open(newline="",encoding="utf-8") as stream:
            rows=list(csv.DictReader(stream))
        learned=[r for r in rows if r["backend"]=="ml_cost_aware"]
        baseline={r["frame"]:r for r in rows if r["backend"]=="sap_rebuild"}
        if len(learned)!=12 or len(baseline)!=12:
            raise RuntimeError("ML regression missing expected 12 frames")
        for row in learned:
            if row["chosen_backend"]!="global_guarded_exact" or row["ml_bypassed"]!="0":
                raise RuntimeError("Linear ML controller did not choose expected action")
            truth=baseline[row["frame"]]
            if row["pair_hash"]!=truth["pair_hash"] or row["pairs"]!=truth["pairs"]:
                raise RuntimeError("ML controller returned an incorrect pair set")
            if int(row["misses"])!=0 or not float(row["ms"])>0:
                raise RuntimeError("ML controller metrics invalid")
            for index in range(12):
                if not row["f"+str(index)]:
                    raise RuntimeError("Missing causal training feature")
        bad=root/"corrupt.mlpc"
        bad.write_text("MALFORMED_MODEL\n",encoding="utf-8")
        run(args.replayer,samples,root/"rejected.csv",0,bad,success=False)
        print("PASS: fixed PyTorch weight ABI, 12 full frame-pair oracles, "
              "safe backend selection and malformed-model rejection")

if __name__=="__main__":
    main()
