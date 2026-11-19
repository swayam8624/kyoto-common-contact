#!/usr/bin/env python3
"""Native Stage-2 RBC research gate: 5 full-pair methods + 4 real solver arms.

Runs on independent NEW immutable material snapshots rather than directly
optimizing old V4 scenes. Every frame verifies ALL five method's full pair
identities against fresh SAP and all coupled solver states against SAP.

Never overwrites an existing output directory. Distinguishes measured
AABB-only query time from real whole-step XPBD-style simulation time.
Results are EXPLORATORY one-shot rotated-order runs, not balanced A/A
confidence intervals or independently established physics fidelity.

Illustrative full cohort:
  python3 scripts/run_rbc_research_gate.py \
    --exporter build/kyoto-final-cpu/ccp_material_export \
    --replayer build/kyoto-final-cpu/ccp_rbc_material_tournament \
    --coupled build/kyoto-final-cpu/ccp_rbc_coupled_tournament \
    --output output/rbc-new-54-scene-run --frames 60 --variants 3 \
    --budgets 128 512 1024 --coupled-frames 60 --audit-coupled 0
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
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_kyoto_heldout_research import HELDOUT
from run_kyoto_decision_coverage import sha256, snapshot_metadata

METHODS = {
    "sap_rebuild", "guard_fixed_exact", "v4_frozen",
    "sap_persistent", "rbc_indexed_repair"
}
PHYSICAL = {"sap_rebuild","global_guard","frozen_v4","rbc_indexed"}
MATERIALS=("cloth","sponge","cushion","soft_ball","spring","mixed")
SCENARIOS=("free","pull","fold")

def run(args:list[str], log:Path, timeout:int=600)->None:
    with log.open("x",encoding="utf-8") as stream:
        stream.write("COMMAND: "+" ".join(args)+"\n")
        stream.flush()
        result=subprocess.run(args, stdout=stream,stderr=subprocess.STDOUT,
                              timeout=timeout,check=False)
    if result.returncode!=0:
        raise RuntimeError(f"Original native process failed ({result.returncode}); {log}")

def positive(value:str)->float:
    f=float(value)
    if not math.isfinite(f) or f<=0:
        raise ValueError("Invalid positive time in native method CSV")
    return f

def geometric(xs:list[float])->float:
    if not xs or min(xs)<=0:
        raise ValueError("Empty or nonpositive native speedup population")
    return math.exp(sum(math.log(x) for x in xs)/len(xs))

def validate_material(path:Path, frames:int, warmup:int)->dict:
    with path.open(newline="",encoding="utf-8") as stream:
        rows=list(csv.DictReader(stream))
    by=defaultdict(dict)
    slots=defaultdict(set)
    for r in rows:
        name=r["method"];f=int(r["frame"])
        if name not in METHODS or f<0 or f>=frames or f in by[name]:
            raise ValueError("Material native method/trajectory coverage wrong")
        if int(r["after_warmup"])!=int(f>=warmup):
            raise ValueError("Native material warmup flag wrong")
        ms=positive(r["query_ms"])
        if ms<=0 or int(r["pairs"])<0 or int(r["pair_fnv64"])<0:
            raise ValueError("Native pair/time receipts invalid")
        slot=int(r["execution_slot"])
        if not 0<=slot<len(METHODS) or slot in slots[f]:
            raise ValueError("Duplicate or invalid native method execution slot")
        slots[f].add(slot)
        by[name][f]=r
    if set(by)!=METHODS or len(rows)!=frames*len(METHODS):
        raise ValueError("Native 5-arm full-pair cohort incomplete")
    if any(set(by[x])!=set(range(frames)) for x in METHODS):
        raise ValueError("Native experiment has missing frames")
    if any(len(slots[f])!=len(METHODS) for f in range(frames)):
        raise ValueError("Native per-frame order missing a policy slot")
    for f in range(frames):
        reference=by["sap_rebuild"][f]
        for name in METHODS:
            row=by[name][f]
            if (row["pairs"],row["pair_fnv64"])!=(
                reference["pairs"],reference["pair_fnv64"]):
                raise ValueError("Native full pair receipt mismatches SAP")
    means={name:sum(positive(by[name][f]["query_ms"])
                    for f in range(warmup,frames))/(frames-warmup)
           for name in METHODS}
    rbc=[by["rbc_indexed_repair"][f] for f in range(frames)]
    v4=[by["v4_frozen"][f] for f in range(frames)]
    return {
        "source":str(path),"sha256":sha256(path),
        "frames":frames,"warmup":warmup,
        "all_five_pair_streams_equal_fresh_sap_on_every_frame":True,
        "actual_method_names":sorted(METHODS),
        "per_method_mean_ms":means,
        "guard_over_rbc":means["guard_fixed_exact"]/means["rbc_indexed_repair"],
        "guard_over_v4":means["guard_fixed_exact"]/means["v4_frozen"],
        "guard_over_persistent_sap":
            means["guard_fixed_exact"]/means["sap_persistent"],
        "rbc_local_repairs":sum(int(r["rbc_local_repairs"]) for r in rbc),
        "rbc_full_builds":sum(int(r["rbc_full_build_delta"]) for r in rbc),
        "rbc_index_tests":sum(int(r["rbc_index_comparisons"]) for r in rbc),
        "rbc_filtered_pair_checks":
            sum(int(r["rbc_query_pair_checks"]) for r in rbc),
        "rbc_potential_pairs_after_final_frame":
            int(rbc[-1]["rbc_potential_pairs"]),
        "frozen_v4_actual_sap_actions":
            sum(int(r["v4_action"]=="sap_persistent_coherent") for r in v4),
        "frozen_v4_trial_frames":sum(int(r["v4_trial"]) for r in v4),
    }

def validate_physics(path:Path,frames:int,scenario:str,
                     geometry:int,topology:int,strict:bool)->dict:
    with path.open(newline="",encoding="utf-8") as stream:
        rows=list(csv.DictReader(stream))
    by=defaultdict(dict)
    frame_slots=defaultdict(set)
    for r in rows:
        name=r["policy"];f=int(r["frame"])
        if name not in PHYSICAL or f<0 or f>=frames or f in by[name]:
            raise ValueError("Native physics policy or frame duplicated/invalid")
        if (r["scenario"]!=scenario or
            int(r["geometry_seed"])!=geometry or
            int(r["topology_seed"])!=topology or
            int(r["strict_audit"])!=int(strict)):
            raise ValueError("Native physical source/strict-audit mismatch")
        slot=int(r["execution_slot"])
        if not 0<=slot<len(PHYSICAL) or slot in frame_slots[f]:
            raise ValueError("Bad or duplicate physical policy slot")
        frame_slots[f].add(slot)
        if float(r["max_position_diff_sap"])>1e-7 or int(r["oracle_misses"])!=0:
            raise ValueError("Physical state/independent candidate oracle mismatch")
        for key in ("step_ms","broad_ms","narrow_ms","max_stretch"):
            value=float(r[key])
            if not math.isfinite(value) or value<0:
                raise ValueError("Nonfinite or negative physical metric")
        if int(r["rbc_queries"])!=5 and name=="rbc_indexed":
            raise ValueError("RBC failed to run five true contact solver queries")
        if strict and name=="rbc_indexed" and not int(r["oracle_checked"]):
            raise ValueError("Strict physics run did not audit RBC candidates")
        by[name][f]=r
    if set(by)!=PHYSICAL or len(rows)!=len(PHYSICAL)*frames:
        raise ValueError("Native physical policy coverage incomplete")
    if any(set(by[x])!=set(range(frames)) for x in PHYSICAL):
        raise ValueError("Native physical frame count incomplete")
    for f in range(frames):
        if len(frame_slots[f])!=len(PHYSICAL):
            raise ValueError("Native physics method order incomplete")
        truth=by["sap_rebuild"][f]
        for name in PHYSICAL:
            r=by[name][f]
            for key in ("candidate_pairs","precise_tests","self_contacts",
                        "sphere_contacts","self_crossings"):
                if r[key]!=truth[key]:
                    raise ValueError(
                        f"Native physics discrete contact data mismatch {key}")
    after=min(8,max(0,frames//8))
    means={name:sum(float(by[name][f]["step_ms"]) for f in range(after,frames))/
           (frames-after) for name in PHYSICAL}
    guarded=means["global_guard"]
    return {
        "source":str(path),"sha256":sha256(path),
        "frames":frames,"scenario":scenario,
        "geometry_seed":geometry,"topology_seed":topology,
        "strict_every_rbc_solver_query":strict,
        "all_sap_reference_particle_states_equal":True,
        "all_last_query_candidate_sets_equal_sap":True,
        "all_last_query_precise_and_contact_counts_equal_sap":True,
        "rbc_actual_contact_queries":frames*5,
        "rbc_strict_oracle_queries":frames*5 if strict else 0,
        "rbc_actual_full_index_builds":
            sum(int(by["rbc_indexed"][f]["rbc_index_builds"])
                for f in range(frames)),
        "rbc_actual_local_repairs":
            sum(int(by["rbc_indexed"][f]["rbc_local_repairs"])
                for f in range(frames)),
        "rbc_pairs_charged":
            sum(int(by["rbc_indexed"][f]["rbc_candidate_checks"])
                for f in range(frames)),
        "solver_mean_full_step_ms_by_policy":means,
        "exploratory_guard_over_rbc_full_step":guarded/means["rbc_indexed"]
            if means["rbc_indexed"]>0 else None,
        "real_sphere_contacts":
            sum(int(by["rbc_indexed"][f]["sphere_contacts"])
                for f in range(frames)),
        "real_same_sheet_self_contacts":
            sum(int(by["rbc_indexed"][f]["self_contacts"])
                for f in range(frames)),
        "WARNING_audited_step_times_include_full_oracle":strict,
    }

def report_document(d:dict)->str:
    scenes=d["native_material_scenes"]
    coupled=d["coupled_physical_scenes"]
    lines=[
        "# RBC Stage-2 full-pair and physically coupled native challenge",
        "",
        "This report contains actual native measurements, not a model-predicted speedup.",
        "All source receipt SHA256 values are in RBC_RESULTS.json.",
        "**No historical evidence or frozen V4 was changed.**",
        "",
        f"Native independent material trajectories: **{len(scenes)}**.",
        f"Source frames per trajectory: **{d['frames']}**, warmup: **{d['warmup']}**.",
        f"Actual full-pair tested methods: **{len(METHODS)}**.",
        "",
        "All five methods were independently executed and generated IDENTICAL",
        "sorted full pair streams to fresh SAP on every material frame,",
        "or the runner would have failed without publishing this report.",
        "",
        "## Equal-scene-weight geometric Guard/method mean query ratios",
        "",
        "| Actually executed CPU method | Guard/method geomean | Scenes faster than Guard |",
        "|---|---:|---:|"
    ]
    for m in sorted(METHODS):
        ratio=geometric([
            scene["result"]["per_method_mean_ms"]["guard_fixed_exact"]/
            scene["result"]["per_method_mean_ms"][m] for scene in scenes])
        wins=sum(scene["result"]["per_method_mean_ms"][m] <
                 scene["result"]["per_method_mean_ms"]["guard_fixed_exact"]
                 for scene in scenes)
        lines.append(f"| {m} | {ratio:.5f}x | {wins}/{len(scenes)} |")
    lines += [
        "",
        "## Per-family/size result (multiple independently seeded inputs)",
        "",
        "| Family | Budget | Actual N range | Scenes | Guard/RBC geomean | Guard/V4 geomean | RBC scene wins |",
        "|---|---:|---|---:|---:|---:|---:|",
    ]
    by=defaultdict(list)
    for scene in scenes:
        by[(scene["material"],scene["budget"])].append(scene)
    for (mat,budget),items in sorted(by.items()):
        n=sorted({x["actual_n"] for x in items})
        ratios=[x["result"]["guard_over_rbc"] for x in items]
        v4=[x["result"]["guard_over_v4"] for x in items]
        lines.append(f"| {mat} | {budget} | {n[0]}..{n[-1]} | {len(items)} | "
                     f"{geometric(ratios):.5f}x | {geometric(v4):.5f}x | "
                     f"{sum(x>1 for x in ratios)}/{len(items)} |")
    lines += [
        "", "## Real coupled physical solver comparison", "",
        f"Scenarios executed: {len(coupled)}. Every scenario ran four",
        "independently stepped solver policy streams with 2 cloth sheets,",
        "2 real moving spheres, five contact queries per physics frame,",
        "identical source seeds and full particle state comparisons.",
        "",
        "| Scenario | Geometry seed | Topology seed | Frames | Strict 5-query RBC audit | Guard/RBC FULL step ratio* | Real sphere contacts | Self contacts | RBC local repairs |",
        "|---|---:|---:|---:|---|---:|---:|---:|---:|",
    ]
    for c in coupled:
        x=c["result"]
        ratio=x["exploratory_guard_over_rbc_full_step"]
        lines.append(f"| {c['scenario']} | {c['geometry']} | {c['topology']} "
                     f"| {c['frames']} | {str(c['audit']).lower()} | "
                     f"{ratio:.4f}x | {x['real_sphere_contacts']} | "
                     f"{x['real_same_sheet_self_contacts']} | "
                     f"{x['rbc_actual_local_repairs']} |")
    lines += [
        "",
        "**Interpretation of the starred full-step ratios:** this is one",
        "rotation-order native session per actual physical scene. If strict",
        "auditing was enabled, RBC's full solver time ALSO includes a",
        "per-iteration independent O(N²) SAP oracle; those ratios cannot be",
        "used as fair performance measurements.",
        "",
        "## Methodological boundaries",
        "",
        "- No A/A repeated runs, balanced ABBA sessions, paired confidence intervals,",
        "  statistical significance or pre-registered unseen physical cohort.",
        "- 54 independent geometry seeds within original material families are NOT",
        "  fully independent source generators/solvers.",
        "- First-frame RBC index construction, dirty repair and output are charged",
        "  inside each query timer. Binary source loading, CSV output and cross-method",
        "  candidate oracle checks are outside the query timers.",
        "- The coupled solver is an experimental XPBD-style spring cloth system;",
        "  it is NOT a calibrated FEM/IPC/OGC solver, and discrete exact pairs",
        "  do NOT guarantee CCD or correct all-time contact handling.",
        "- Complete physics state equality means a faster BROAD PHASE does NOT",
        "  intrinsically change visible fold quality. Claims of reduced folding",
        "  require a separate, independently demonstrated solver improvement.",
        "- A negative or parity result remains in this report. DO NOT tune",
        "  the proposed RBC skin or index heuristic against this report",
        "  and then call the scenes held out.",
        "- A fast RBC material replay without full-step physics improvement",
        "  is a useful engineering result, but not publication-level proof.",
    ]
    return "\n".join(lines)+"\n"

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exporter",required=True,type=Path)
    p.add_argument("--replayer",required=True,type=Path)
    p.add_argument("--coupled",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--materials",nargs="+",default=list(MATERIALS))
    p.add_argument("--regimes",nargs="+",default=["rapid"])
    p.add_argument("--budgets",nargs="+",type=int,default=[128,512,1024])
    p.add_argument("--variants",type=int,default=3)
    p.add_argument("--frames",type=int,default=60)
    p.add_argument("--warmup",type=int,default=8)
    p.add_argument("--skin",type=float,default=.045)
    p.add_argument("--coupled-frames",type=int,default=60)
    p.add_argument("--coupled-scenarios",nargs="+",default=list(SCENARIOS))
    p.add_argument("--coupled-variants",type=int,default=3)
    p.add_argument("--audit-coupled",type=int,choices=[0,1],default=0)
    p.add_argument("--timeout",type=int,default=600)
    a=p.parse_args()
    if a.output.exists():
        p.error("Never overwrite an existing RBC research evidence directory")
    if not all(x.is_file() for x in (a.exporter,a.replayer,a.coupled)):
        p.error("Native source generator, replay and solver tournament must be built")
    if any(x not in MATERIALS for x in a.materials):
        p.error("invalid materials")
    if any(x not in ("baseline","compression","shear","impact","rapid","relax")
           for x in a.regimes):
        p.error("invalid motion regime")
    if any(x not in SCENARIOS for x in a.coupled_scenarios):
        p.error("invalid real solver scene")
    if not 1<=a.variants<=len(HELDOUT) or not 1<=a.coupled_variants<=len(HELDOUT):
        p.error("invalid independently seeded variant count")
    if any(x<32 or x>50000 for x in a.budgets):
        p.error("invalid material budget")
    if not 8<=a.frames<=300 or not 0<=a.warmup<a.frames:
        p.error("invalid frames/warmup")
    if not 8<=a.coupled_frames<=160:
        p.error("invalid coupled frames")
    if not 0<=a.skin<=10 or not math.isfinite(a.skin):
        p.error("invalid finite certificate skin")
    a.output.mkdir(parents=True,exist_ok=False)
    for path in ("snapshots","material_csv","coupled_csv","logs"):
        (a.output/path).mkdir()
    executables={
        "exporter":str(a.exporter.resolve()),
        "material":str(a.replayer.resolve()),
        "coupled":str(a.coupled.resolve()),
    }
    results=[]
    for material in a.materials:
        for regime in a.regimes:
            for budget in a.budgets:
                for variant in HELDOUT[:a.variants]:
                    label=f"{material}-{regime}-b{budget}-g{variant['geometry']}-t{variant['topology']}"
                    snapshot=a.output/"snapshots"/f"{label}.kmcb"
                    trace=a.output/"material_csv"/f"{label}.csv"
                    run([executables["exporter"],str(snapshot),material,str(budget),
                         str(a.frames),str(variant["motion"]),regime,
                         str(variant["geometry"]),str(variant["topology"])],
                        a.output/"logs"/f"{label}-export.log",a.timeout)
                    metadata=snapshot_metadata(snapshot)
                    if metadata["frames"]!=a.frames or metadata["seed"]!=variant["motion"]:
                        raise ValueError("Native material header does not match requested source")
                    run([executables["material"],str(snapshot),str(trace),
                         str(a.warmup),str(a.skin)],
                        a.output/"logs"/f"{label}-replay.log",a.timeout)
                    datum=validate_material(trace,a.frames,a.warmup)
                    results.append({
                        "material":material,"regime":regime,"budget":budget,
                        "actual_n":metadata["n"],**variant,
                        "source":str(snapshot),"source_sha256":sha256(snapshot),
                        "result":datum,
                    })
                    print("PASS RBC MATERIAL",label,"N",metadata["n"],
                          f"Guard/RBC={datum['guard_over_rbc']:.5f}x",
                          f"Guard/V4={datum['guard_over_v4']:.5f}x",
                          "repairs",datum["rbc_local_repairs"],flush=True)
    coupled=[]
    for var in HELDOUT[:a.coupled_variants]:
        for scenario in a.coupled_scenarios:
            label=f"{scenario}-g{var['geometry']}-t{var['topology']}"
            csvpath=a.output/"coupled_csv"/f"{label}.csv"
            run([executables["coupled"],str(csvpath),
                 str(a.coupled_frames),scenario,
                 str(var["geometry"]),str(var["topology"]),
                 str(a.audit_coupled)],
                a.output/"logs"/f"{label}-coupled.log",a.timeout)
            result=validate_physics(csvpath,a.coupled_frames,scenario,
                                    var["geometry"],var["topology"],
                                    bool(a.audit_coupled))
            coupled.append({
                "scenario":scenario,"geometry":var["geometry"],
                "topology":var["topology"],"frames":a.coupled_frames,
                "audit":bool(a.audit_coupled),"result":result,
            })
            print("PASS RBC COUPLED",label,
                  "actual solver frame parity",a.coupled_frames,
                  "RBC repairs",result["rbc_actual_local_repairs"],
                  "contact corrections",result["real_sphere_contacts"],
                  flush=True)
    result={
        "schema":"kyoto-rbc-stage2-real-native-evidence-v1",
        "research_branch":"research/reach-budget-calculus",
        "baseline_frozen_v4_untouched":True,
        "old_sources_and_evidence_overwritten":False,
        "host_platform":platform.platform(),
        "original_natively_executed_binaries_sha256":{
            key:sha256(Path(value)) for key,value in executables.items()},
        "frames":a.frames,"warmup":a.warmup,"skin":a.skin,
        "source_total_material_frames":a.frames*len(results),
        "source_total_physical_frames":a.coupled_frames*len(coupled),
        "material_source_families":list(a.materials),
        "material_motion_regimes":list(a.regimes),
        "native_material_scenes":results,
        "coupled_physical_scenes":coupled,
        "warnings":[
            "single native session, method order rotated not statistically balanced ABBA",
            "no prior art novelty claim or independent calibrated solver",
            "no physical full-step win inferred from AABB-only candidate timing",
            "strict audited physical step_ms includes independent quadratic oracle",
            "no self-contact claim unless observed nonzero in original response counts",
        ],
    }
    (a.output/"RBC_RESULTS.json").write_text(
        json.dumps(result,indent=2)+"\n",encoding="utf-8")
    (a.output/"RBC_RESULTS.md").write_text(
        report_document(result),encoding="utf-8")
    print("PASS RBC FULL SOURCE-VERIFIED SCIENTIFIC GATE",
          len(results),"material trajectories",
          len(coupled),"physical solver scenarios",
          "RBC results",a.output/"RBC_RESULTS.md",flush=True)
if __name__=="__main__":
    main()
