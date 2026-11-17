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
    # If a CMake build failed, the old executable may still exist on disk.
    # Explicitly reject an obsolete replay ABI before any oracle fixtures.
    signature=subprocess.run([str(args.replayer),"--capabilities"],
                             capture_output=True,text=True,timeout=10,check=False)
    required={"KYOTO_REPLAY_ABLATION_V6","sap_persistent_coherent",
              "ml_cost_aware_conservative","ml_bookkeeping_ms","ml_wrapper_ms",
              "guard_sap_measured_probe","guard_sap_bounded_trial",
              "hybrid_probed","hybrid_switched","field_events",
              "randomized_method_order","global_guarded_aa_control",
              "bvh_refit_exact","bvh_adaptive_exact","grid_spatial_hash_exact",
              "delta_spatial_join_exact","patch_certificate_fixed",
              "patch_certificate_signal","unified_certified_hierarchy_v0"}
    if signature.returncode!=0 or not required.issubset(set(signature.stdout.split())):
        raise RuntimeError("Replay executable is stale; rebuild it with the current learned_online.hpp")
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
        new_exact=(
            "bvh_refit_exact","bvh_adaptive_exact","grid_spatial_hash_exact",
            "delta_spatial_join_exact","patch_certificate_fixed",
            "patch_certificate_signal","unified_certified_hierarchy_v0",
            "global_guarded_aa_control")
        reference={r["frame"]:r for r in rows if r["backend"]=="sap_rebuild"}
        for label in new_exact:
            matching=[r for r in rows if r["backend"]==label]
            if len(matching)!=12:
                raise RuntimeError(f"{label}: expected 12 exact benchmark frames")
            for row in matching:
                expected=reference[row["frame"]]
                if row["pairs"]!=expected["pairs"] or row["pair_hash"]!=expected["pair_hash"]:
                    raise RuntimeError(f"{label}: exact candidate pairs differ")
        # Repeated same-snapshot randomized method orders must remain correct.
        # The main parser supports the old positional interface as well.
        random_report=root/"randomized.csv"
        run(args.replayer,samples,random_report,0,model,987654321)
        with random_report.open(newline="",encoding="utf-8") as stream:
            randomized=list(csv.DictReader(stream))
        unique_before=list(dict.fromkeys(r["backend"] for r in rows))
        unique_after=list(dict.fromkeys(r["backend"] for r in randomized))
        if set(unique_before)!=set(unique_after) or unique_before==unique_after:
            raise RuntimeError("Method ordering was not randomized reproducibly")
        expected_hash={(r["backend"],r["frame"]):r["pair_hash"] for r in rows}
        for row in randomized:
            if row["pair_hash"]!=expected_hash[(row["backend"],row["frame"])]:
                raise RuntimeError("Randomization invalidated pair correctness")
        persistent=[r for r in rows if r["backend"]=="sap_persistent_coherent"]
        if len(persistent)!=12:
            raise RuntimeError("Temporally coherent persistent SAP baseline missing")
        oracle_by_frame={r["frame"]:r for r in rows if r["backend"]=="sap_rebuild"}
        for row in persistent:
            truth=oracle_by_frame[row["frame"]]
            if row["pairs"]!=truth["pairs"] or row["pair_hash"]!=truth["pair_hash"]:
                raise RuntimeError("Persistent SAP lost exact candidate pair equivalence")
        bounded_trial=[r for r in rows if r["backend"]=="guard_sap_bounded_trial"]
        if len(bounded_trial)!=12:
            raise RuntimeError("Bounded single-backend Guard/SAP policy missing")
        for row in bounded_trial:
            truth=oracle_by_frame[row["frame"]]
            if row["pair_hash"]!=truth["pair_hash"] or row["pairs"]!=truth["pairs"]:
                raise RuntimeError("Bounded Guard/SAP trial lost exact AABB-pair parity")
            if row["chosen_backend"] not in (
                    "global_guarded_exact","sap_persistent_coherent"):
                raise RuntimeError("Bounded Guard/SAP trial selected invalid backend")
        hybrid=[r for r in rows if r["backend"]=="guard_sap_measured_probe"]
        if len(hybrid)!=12:
            raise RuntimeError("Causal Guard-SAP probe policy missing")
        for row in hybrid:
            truth=oracle_by_frame[row["frame"]]
            if row["pair_hash"]!=truth["pair_hash"] or row["pairs"]!=truth["pairs"]:
                raise RuntimeError("Guard-SAP hybrid lost exact AABB-pair parity")
            if row["chosen_backend"] not in (
                    "global_guarded_exact","sap_persistent_coherent"):
                raise RuntimeError("Guard-SAP hybrid returned unknown action")
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
        # Adversarial high-motion material: the persistent order changes enough
        # to exercise bounded insertion repair and occasional full-sort fallback.
        rapid=root/"rapid.kmcb"
        rapid_report=root/"rapid.csv"
        run(args.exporter,rapid,"mixed",512,20,89211,"rapid")
        run(args.replayer,rapid,rapid_report,0,model)
        with rapid_report.open(newline="",encoding="utf-8") as source:
            rapid_rows=list(csv.DictReader(source))
        rapid_oracle={r["frame"]:r for r in rapid_rows
                      if r["backend"]=="sap_rebuild"}
        rapid_bounded=[r for r in rapid_rows if r["backend"]=="guard_sap_bounded_trial"]
        if len(rapid_bounded)!=20:
            raise RuntimeError("Rapid bounded Guard/SAP trial missing")
        for row in rapid_bounded:
            truth=rapid_oracle[row["frame"]]
            if row["pair_hash"]!=truth["pair_hash"] or row["pairs"]!=truth["pairs"]:
                raise RuntimeError("Bounded Guard/SAP trial lost rapid-motion pair parity")
        rapid_hybrid=[r for r in rapid_rows if r["backend"]=="guard_sap_measured_probe"]
        if len(rapid_hybrid)!=20:
            raise RuntimeError("Rapid-motion causal hybrid evidence missing")
        for row in rapid_hybrid:
            ref=rapid_oracle[row["frame"]]
            if row["pairs"]!=ref["pairs"] or row["pair_hash"]!=ref["pair_hash"]:
                raise RuntimeError("Causal Guard-SAP hybrid failed under rapid deformation")
        rapid_persistent=[r for r in rapid_rows
                          if r["backend"]=="sap_persistent_coherent"]
        if len(rapid_persistent)!=20:
            raise RuntimeError("Rapid-motion persistent SAP baseline missing")
        for r in rapid_persistent:
            ref=rapid_oracle[r["frame"]]
            if (r["pairs"]!=ref["pairs"] or r["pair_hash"]!=ref["pair_hash"]
                    or int(r["misses"])!=0):
                raise RuntimeError("Persistent SAP failed in rapidly deforming scene")
        bad=root/"corrupt.mlpc"
        bad.write_text("MALFORMED_MODEL\n",encoding="utf-8")
        run(args.replayer,samples,root/"rejected.csv",0,bad,success=False)
        print("PASS: direct-Bounds learned wrapper, optimized small-N Guard "
              "fallback, hierarchical region parity, 38 fully compared "
              "oracle frames across both original/conservative variants, detailed timing fields and malformed-model rejection")

if __name__=="__main__":
    main()
