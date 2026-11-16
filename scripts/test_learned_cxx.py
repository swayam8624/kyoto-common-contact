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

def create_model(path, biases="0 -3 -2 -1"):
    # depth=0 means a single Linear(12,4), all weights are zero. The bias
    # makes Global Guard the cheapest on every frame.
    body=["KYOTO_MLP_V1","12 1 0 4",
          " ".join(["0"]*12)," ".join(["1"]*12),
          " ".join(["0"]*(12*4)),
          biases]
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
        if any(None in r for r in rows):
            raise RuntimeError("C++ produced extra CSV fields absent from header")
        for required in ("ml_bypassed","ml_feature_ms","ml_inference_ms",
                         "ml_decision_ms","ml_backend_ms","ml_bookkeeping_ms",
                         "ml_wrapper_ms"):
            if required not in rows[0]:
                raise RuntimeError(f"Missing ML performance column: {required}")
        learned=[r for r in rows if r["backend"]=="ml_cost_aware"]
        baseline={r["frame"]:r for r in rows if r["backend"]=="sap_rebuild"}
        conservative=[r for r in rows if r["backend"]=="ml_cost_aware_conservative"]
        if len(conservative)!=12 or len(learned)!=12 or len(baseline)!=12:
            raise RuntimeError("ML regression missing expected 12 frames")
        for row in learned:
            if row["chosen_backend"]!="global_guarded_exact" or row["ml_bypassed"]!="0":
                raise RuntimeError("Linear ML controller did not choose expected action")
            truth=baseline[row["frame"]]
            if row["pair_hash"]!=truth["pair_hash"] or row["pairs"]!=truth["pairs"]:
                raise RuntimeError("ML controller returned an incorrect pair set")
            if int(row["misses"])!=0 or not float(row["ms"])>0:
                raise RuntimeError("ML controller metrics invalid")
            if any(float(row[key])<0 for key in ("ml_bookkeeping_ms","ml_wrapper_ms")):
                raise RuntimeError("Negative learned post-backend or wrapper time")
            for index in range(12):
                if not row["f"+str(index)]:
                    raise RuntimeError("Missing causal training feature")
        # The 128-primitive fallback must never pay neural prediction; its
        # model intentionally ranks hierarchical region as cheaper than Guard.
        # This also guards the optimized direct-Bounds entry against losses of
        # pair/oracle coverage when switching the original input representation.
        alternate=root/"prefer_region.mlpc"
        create_model(alternate,"0 -3 -2 -4")
        small=root/"small.kmcb"
        small_report=root/"small.csv"
        run(args.exporter,small,"cloth",128,14,4321,"impact")
        run(args.replayer,small,small_report,0,alternate)
        with small_report.open(newline="",encoding="utf-8") as source:
            small_rows=list(csv.DictReader(source))
        by_frame={r["frame"]:r for r in small_rows
                  if r["backend"]=="sap_rebuild"}
        bypass_rows=[r for r in small_rows if r["backend"]=="ml_cost_aware"]
        if len(bypass_rows)!=14:
            raise RuntimeError("Small-N bypass is missing frames")
        for r in bypass_rows:
            ref=by_frame[r["frame"]]
            if (r["chosen_backend"]!="global_guarded_exact"
                    or r["ml_bypassed"]!="1"
                    or r["pairs"]!=ref["pairs"]
                    or r["pair_hash"]!=ref["pair_hash"]):
                raise RuntimeError("Small-N direct-Bounds fallback lost pair parity")

        # Exercise the newly constructed region-primitive path too: the
        # C++ runner supplies Bounds and the selected algorithm needs regions.
        region_report=root/"region.csv"
        run(args.replayer,samples,region_report,0,alternate)
        with region_report.open(newline="",encoding="utf-8") as source:
            region_rows=list(csv.DictReader(source))
        region_by_frame={r["frame"]:r for r in region_rows
                         if r["backend"]=="sap_rebuild"}
        selected=[r for r in region_rows if r["backend"]=="ml_cost_aware"]
        if len(selected)!=12:
            raise RuntimeError("Region backend not evaluated on 12 frames")
        for r in selected:
            ref=region_by_frame[r["frame"]]
            if (r["chosen_backend"]!="region_hierarchical"
                    or r["ml_bypassed"]!="0"
                    or r["pairs"]!=ref["pairs"]
                    or r["pair_hash"]!=ref["pair_hash"]):
                raise RuntimeError("Direct-Bounds hierarchical region lost pair parity")

        # The conservative policy starts with Guard and must preserve the
        # exact pair oracle even when the model prefers hierarchical region.
        cons=[r for r in region_rows
              if r["backend"]=="ml_cost_aware_conservative"]
        if len(cons)!=12:
            raise RuntimeError("Conservative learned controller missing frames")
        for r in cons:
            ref=region_by_frame[r["frame"]]
            if (r["chosen_backend"]!="global_guarded_exact"
                    or r["pairs"]!=ref["pairs"] or r["pair_hash"]!=ref["pair_hash"]):
                raise RuntimeError("Guard-first conservative policy lost pair parity")
        bad=root/"corrupt.mlpc"
        bad.write_text("MALFORMED_MODEL\n",encoding="utf-8")
        run(args.replayer,samples,root/"rejected.csv",0,bad,success=False)
        print("PASS: direct-Bounds learned wrapper, optimized small-N Guard "
              "fallback, hierarchical region parity, 38 fully compared "
              "oracle frames across both original/conservative variants, detailed timing fields and malformed-model rejection")

if __name__=="__main__":
    main()
