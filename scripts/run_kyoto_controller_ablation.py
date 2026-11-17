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
import platform
from pathlib import Path
import random
import subprocess
import time
from collections import Counter
from datetime import datetime, timezone
from statistics import mean

try:
    from .evaluate_learned_replay import evaluate, render
except ImportError:
    from evaluate_learned_replay import evaluate, render

MATERIALS=("cloth","sponge","cushion","soft_ball","spring","mixed")
REGIMES=("baseline","impact","shear","compression","rapid","relax")
BUDGETS=(128,512,2048)
DEFAULT_SEEDS=(1201,1301,1409,1511)
TIMES=("ml_feature_ms","ml_inference_ms","ml_decision_ms","ml_backend_ms",
       "ml_bookkeeping_ms","ml_wrapper_ms")
REPLAY_ABI="KYOTO_REPLAY_ABLATION_V5"
REQUIRED_POLICIES=frozenset(("sap_rebuild","sap_persistent_coherent",
    "global_guarded_exact","ml_cost_aware","ml_cost_aware_conservative",
    "guard_sap_measured_probe","guard_sap_bounded_trial",
    "global_guarded_aa_control","bvh_refit_exact","bvh_adaptive_exact",
    "grid_spatial_hash_exact","delta_spatial_join_exact",
    "patch_certificate_fixed","patch_certificate_signal"))
REQUIRED_COLUMNS=frozenset(("backend","frame","ms","pairs","pair_hash","misses",
    "chosen_backend","ml_switched","ml_bypassed",
    "hybrid_probed","hybrid_switched","field_events",*TIMES))
REQUIRED_CAPABILITIES=frozenset((REPLAY_ABI,"sap_persistent_coherent",
    "ml_cost_aware_conservative","ml_bookkeeping_ms","ml_wrapper_ms",
    "guard_sap_measured_probe","guard_sap_bounded_trial",
    "hybrid_probed","hybrid_switched","field_events",
    "randomized_method_order","global_guarded_aa_control",
    "bvh_refit_exact","bvh_adaptive_exact","grid_spatial_hash_exact",
    "delta_spatial_join_exact","patch_certificate_fixed",
    "patch_certificate_signal"))


def verify_replayer(replayer: Path, root: Path) -> None:
    """Fail before generating data if compilation failed or the binary is old.

    A failed 'cmake --build' leaves the previous executable on disk. Merely
    checking Path.is_file() cannot detect that dangerous stale-binary case.
    """
    # Track every directly related interface, not only the leaf controller.
    # In particular, Result::mlBookkeepingMs is declared in generic_online.hpp;
    # updating learned_online.hpp without it caused a real macOS build failure.
    sources=[root/"apps/material_replay_benchmark.cpp",
             root/"include/ccp/learned_online.hpp",
             root/"include/ccp/generic_online.hpp",
             root/"include/ccp/learned_policy.hpp",
             root/"include/ccp/live_cloth.hpp"]
    for src in sources:
        if src.is_file() and src.stat().st_mtime_ns>replayer.stat().st_mtime_ns:
            raise RuntimeError(
                f"Replay executable is older than {src}. "
                "Rebuild ccp_material_replay successfully before benchmarking.")
    try:
        probe=subprocess.run([str(replayer),"--capabilities"],
                             capture_output=True,text=True,timeout=10,check=False)
    except (OSError,subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f"Cannot probe replay executable: {replayer}") from exc
    tokens=set(probe.stdout.split())
    if probe.returncode!=0 or not REQUIRED_CAPABILITIES.issubset(tokens):
        raise RuntimeError(
            "Replay executable is outdated or missing required capabilities "
            f"({REPLAY_ABI}); stop and rebuild from the synced "
            "include/ccp/learned_online.hpp and apps/material_replay_benchmark.cpp. "
            f"Executable: {replayer}; stdout={probe.stdout!r}; "
            f"stderr={probe.stderr[:350]!r}")


def verify_replay_csv(path: Path) -> None:
    """Never silently reuse an old replay after a failed build/resume."""
    with path.open(newline="",encoding="utf-8") as stream:
        reader=csv.DictReader(stream)
        columns=set(reader.fieldnames or [])
        missing=REQUIRED_COLUMNS-columns
        if missing:
            raise RuntimeError(
                f"Stale or incomplete replay CSV {path}: missing {sorted(missing)}. "
                "Preserve this output and use a NEW --output directory after rebuilding.")
        observed={row["backend"] for row in reader}
        missing_policies=REQUIRED_POLICIES-observed
        if missing_policies:
            raise RuntimeError(
                f"Stale replay CSV {path}: missing policies {sorted(missing_policies)}. "
                "Use a NEW --output directory; old results are not valid "
                "for the coherent-SAP/controller ablation.")



def choose_combos(seeds: list[int], materials: list[str],
                  regimes: list[str], budgets: list[int],
                  sample_seed: int, max_experiments: int,
                  sampling: str) -> list[tuple[int,str,str,int]]:
    """Predeclared sample; balanced covers each material/regime/budget
    cell once before a second seed can enter that cell. No outcome labels
    or previously measured runtimes influence this selection.
    """
    combos=list(itertools.product(sorted(seeds),sorted(set(materials)),
                                  sorted(set(regimes)),sorted(set(budgets))))
    rng=random.Random(sample_seed)
    if sampling=="random":
        rng.shuffle(combos)
    elif sampling=="balanced":
        groups={}
        for combo in combos:
            groups.setdefault(combo[1:],[]).append(combo)
        for key in groups:
            rng.shuffle(groups[key])
        keys=sorted(groups)
        rng.shuffle(keys)
        combos=[]
        for sample_idx in range(len(seeds)):
            for key in keys:
                group=groups[key]
                if sample_idx<len(group):
                    combos.append(group[sample_idx])
    else:
        raise ValueError(f"Unknown sampling: {sampling}")
    return combos[:max_experiments] if max_experiments else combos


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
    parser.add_argument("--sampling",choices=("random","balanced"),default="random",
                        help="balanced covers all material/regime/size cells before any repeat")
    parser.add_argument("--bootstrap",type=int,default=2000)
    parser.add_argument("--timeout",type=int,default=600)
    parser.add_argument("--repeats",type=int,default=1,
                        help="Repeat each *identical* snapshot under a new independently randomized backend order")
    parser.add_argument("--order-seed",type=int,default=20261118,
                        help="Predeclared RNG seed for deterministic reproducible policy ordering")
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
    # Fail BEFORE creating an output directory or any material snapshots.
    try:
        verify_replayer(replayer,Path(__file__).resolve().parent.parent)
    except RuntimeError as exc:
        parser.error(str(exc))
    if args.frames<=args.warmup+4 or args.warmup<0 or args.bootstrap<100:
        parser.error("Need frames > warmup + 4, warmup>=0, and bootstrap>=100")
    if any(s<0 for s in args.seeds) or len(set(args.seeds))!=len(args.seeds):
        parser.error("Seeds must be distinct nonnegative integers")
    if set(args.seeds)&{101,1009}:
        parser.error("Seeds 101 and 1009 were already inspected as test: choose fresh seeds")
    if any(b<1 or b>50000 for b in args.budgets):
        parser.error("Primitive budgets must be in 1..50000 (exporter may impose tighter limits)")
    if args.repeats<1 or args.order_seed<0 or args.max_experiments<0 or args.timeout<10:
        parser.error("Invalid max experiments or timeout")
    combos=choose_combos(args.seeds,args.materials,args.regimes,args.budgets,
                        args.sample_seed,args.max_experiments,args.sampling)
    if not combos:parser.error("Empty experiment matrix")
    contract={"model_sha256":hashlib.sha256(model.read_bytes()).hexdigest(),
              "replayer_sha256":hashlib.sha256(replayer.read_bytes()).hexdigest(),
              "exporter_sha256":hashlib.sha256(exporter.read_bytes()).hexdigest(),
              "replayer_abi":REPLAY_ABI,
              "environment":{"platform":platform.platform(),
                             "machine":platform.machine(),
                             "processor":platform.processor(),
                             "python":platform.python_version(),
                             "logical_cpus":os.cpu_count()},
              "source_model_path":str(model),"seeds":sorted(args.seeds),
              "materials":sorted(set(args.materials)),"regimes":sorted(set(args.regimes)),
              "budgets":sorted(set(args.budgets)),"frames":args.frames,
              "warmup":args.warmup,"max_experiments":args.max_experiments,
              "sample_seed":args.sample_seed,"sampling":args.sampling,
              "repeats":args.repeats,"order_seed":args.order_seed,
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
    # Every repeat uses the EXACT same exported binary snapshot and a new
    # independently randomized algorithm order. The stable digest is part of
    # the protocol, never dependent on Python's randomized hash() function.
    for i,(seed,material,regime,budget) in enumerate(combos,1):
        stem=f"actual-{material}-{regime}-n{budget}-seed{seed}"
        snapshot=output/"snapshots"/f"{stem}.kmcb"
        print(f"[ABLATION] {i}/{len(combos)}: {stem}, repeats {args.repeats}",flush=True)
        if not snapshot.exists():
            tmp=snapshot.with_name(snapshot.name+".partial")
            try:
                checked([exporter,tmp,material,budget,args.frames,seed,regime],
                        output/"logs"/f"{stem}-export.log",args.timeout)
                os.replace(tmp,snapshot)
            finally:
                tmp.unlink(missing_ok=True)
        for repeat in range(args.repeats):
            suffix=f"-rep{repeat+1}" if args.repeats>1 else ""
            report=output/"replays"/f"{stem}{suffix}-evaluated.csv"
            key=f"{args.order_seed}|{stem}|{repeat}".encode("utf-8")
            order_seed=int.from_bytes(hashlib.sha256(key).digest()[:8],"little") or 1
            if report.exists():
                verify_replay_csv(report)
            else:
                tmp=report.with_name(report.name+".partial")
                try:
                    checked([replayer,snapshot,tmp,0,model,order_seed],
                            output/"logs"/f"{stem}{suffix}-replay.log",args.timeout)
                    verify_replay_csv(tmp)
                    os.replace(tmp,report)
                finally:
                    tmp.unlink(missing_ok=True)
            paths.append(report)
    scenes,summary=evaluate(paths,args.warmup,args.bootstrap,args.sample_seed)
    # These are existing-implementation relative runtimes, not predicted policy labels.
    ratio_total=sum(s["measured_mean_ms"]["global_guarded_exact"] for s in scenes)/sum(
        s["measured_mean_ms"]["ml_cost_aware"] for s in scenes)
    summary["aggregate_total_scene_mean_guard_over_ml"]=ratio_total
    # An independent policy variant is evaluated on exactly the same
    # replay frames: guard-first, stronger hysteresis, longer cooldown.
    # Treat this as a predeclared exploratory A/B result, not a free win.
    if all("ml_cost_aware_conservative" in s["measured_mean_ms"] for s in scenes):
        guard_ratio=[
            s["measured_mean_ms"]["global_guarded_exact"]/
            s["measured_mean_ms"]["ml_cost_aware_conservative"]
            for s in scenes
        ]
        default_ratio=[
            s["measured_mean_ms"]["ml_cost_aware"]/
            s["measured_mean_ms"]["ml_cost_aware_conservative"]
            for s in scenes
        ]
        summary["conservative_vs_global_geomean"]=math.exp(mean(map(math.log,guard_ratio)))
        summary["conservative_vs_default_ml_geomean"]=math.exp(mean(map(math.log,default_ratio)))
        summary["conservative_vs_global_scene_wins"]=sum(x>1 for x in guard_ratio)
        summary["conservative_vs_default_scene_wins"]=sum(x>1 for x in default_ratio)
        summary["conservative_vs_global_total_scene_mean"]=(sum(
            s["measured_mean_ms"]["global_guarded_exact"] for s in scenes)/sum(
            s["measured_mean_ms"]["ml_cost_aware_conservative"] for s in scenes))
    # Persistent SAP is an independent temporally coherent baseline, not
    # a neural-controller action. Evaluate it alongside Guard, including
    # the controller's inability to consistently beat this alternative.
    if all("sap_persistent_coherent" in scene["measured_mean_ms"] for scene in scenes):
        coherent_vs_global=[
            scene["measured_mean_ms"]["global_guarded_exact"]/
            scene["measured_mean_ms"]["sap_persistent_coherent"]
            for scene in scenes]
        learned_vs_coherent=[
            scene["measured_mean_ms"]["sap_persistent_coherent"]/
            scene["measured_mean_ms"]["ml_cost_aware"]
            for scene in scenes]
        summary["coherent_sap_vs_global_geomean"]=math.exp(mean(map(math.log,coherent_vs_global)))
        summary["coherent_sap_vs_global_scene_wins"]=sum(v>1 for v in coherent_vs_global)
        summary["coherent_sap_vs_global_total_scene_mean"]=(sum(
            scene["measured_mean_ms"]["global_guarded_exact"] for scene in scenes)/sum(
            scene["measured_mean_ms"]["sap_persistent_coherent"] for scene in scenes))
        summary["learned_vs_coherent_sap_geomean"]=math.exp(mean(map(math.log,learned_vs_coherent)))
        summary["learned_vs_coherent_sap_scene_wins"]=sum(v>1 for v in learned_vs_coherent)
        summary["comparator_warning"]=("Persistent SAP has its own continuously maintained sort "
            "and is a standalone algorithm; it was not an available action in the trained 4-output MLP.")
    # Exploratory causal hybrid; all actual probe times remain included.
    if all("guard_sap_measured_probe" in e["measured_mean_ms"] for e in scenes):
        hybrid_ratios=[
            e["measured_mean_ms"]["global_guarded_exact"]/
            e["measured_mean_ms"]["guard_sap_measured_probe"]
            for e in scenes]
        summary["guard_sap_probe_vs_global_geomean"]=math.exp(mean(map(math.log,hybrid_ratios)))
        summary["guard_sap_probe_vs_global_scene_wins"]=sum(v>1 for v in hybrid_ratios)
        summary["guard_sap_probe_vs_global_total_scene_mean"]=(
            sum(e["measured_mean_ms"]["global_guarded_exact"] for e in scenes)/
            sum(e["measured_mean_ms"]["guard_sap_measured_probe"] for e in scenes))
        summary["guard_sap_probe_vs_ml_conservative_geomean"]=math.exp(mean(
            math.log(e["measured_mean_ms"]["ml_cost_aware_conservative"]/
                     e["measured_mean_ms"]["guard_sap_measured_probe"])
            for e in scenes))
        summary["guard_sap_probe_note"]=("Only causal online probes, never access to future "
            "trajectories or hindsight labels; both backend probe wall costs are charged. "
            "This is a new exploratory controller, not proof of publishable superiority.")
        # Instrument the new policy on the same warmed frames, with no
        # accidental counting of learned-controller switch columns.
        probes=0
        hybrid_switches=0
        hybrid_sap_frames=0
        for csv_path in paths:
            with csv_path.open(newline="",encoding="utf-8") as stream:
                for row in csv.DictReader(stream):
                    if row["backend"]!="guard_sap_measured_probe" or int(row["frame"])<args.warmup:
                        continue
                    probes+=int(row["hybrid_probed"])
                    hybrid_switches+=int(row["hybrid_switched"])
                    hybrid_sap_frames+=int(row["chosen_backend"]=="sap_persistent_coherent")
        summary["guard_sap_probe_count"]=probes
        summary["guard_sap_probe_switches"]=hybrid_switches
        summary["guard_sap_probe_sap_frames"]=hybrid_sap_frames
    # Each method has its own live cache; all trials are charged. A new
    # policy never gets to substitute measured time from a separate backend.
    if all("guard_sap_bounded_trial" in e["measured_mean_ms"] for e in scenes):
        trial_ratios=[e["measured_mean_ms"]["global_guarded_exact"]/
                      e["measured_mean_ms"]["guard_sap_bounded_trial"]
                      for e in scenes]
        summary["guard_sap_bounded_trial_vs_global_geomean"]=math.exp(
            mean(map(math.log,trial_ratios)))
        summary["guard_sap_bounded_trial_vs_global_scene_wins"]=sum(
            r>1 for r in trial_ratios)
        summary["guard_sap_bounded_trial_vs_global_total_scene_mean"]=(
            sum(e["measured_mean_ms"]["global_guarded_exact"] for e in scenes)/
            sum(e["measured_mean_ms"]["guard_sap_bounded_trial"] for e in scenes))
        summary["bounded_trial_vs_conservative_ml_geomean"]=math.exp(mean(
            math.log(e["measured_mean_ms"]["ml_cost_aware_conservative"]/
                     e["measured_mean_ms"]["guard_sap_bounded_trial"])
            for e in scenes))
        active=trial_frames=switches=0
        for csv_path in paths:
            with csv_path.open(newline="",encoding="utf-8") as stream:
                for row in csv.DictReader(stream):
                    if row["backend"]!="guard_sap_bounded_trial" or int(row["frame"])<args.warmup:
                        continue
                    active+=int(row["chosen_backend"]=="sap_persistent_coherent")
                    trial_frames+=int(row["hybrid_probed"])
                    switches+=int(row["hybrid_switched"])
        summary["guard_sap_bounded_trial_sap_frames"]=active
        summary["guard_sap_bounded_trial_trial_frames"]=trial_frames
        summary["guard_sap_bounded_trial_switches"]=switches
        summary["bounded_trial_warning"]=(
            "No future information; one backend executes per frame. "
            "Initial Guard costs and later SAP costs are from different "
            "cache states; these are exploratory causal observations.")
    summary["model_sha256"]=contract["model_sha256"]
    summary["fresh_seeds"]=sorted({x[0] for x in combos})
    summary["interpretation"]=("Exploratory fresh-seed pilot, no retuning on these scenes; "
        "scene-bootstrap interval does not give seed-independent uncertainty; "
        "use RUMBLE_PATTERNS.json seed-blocked intervals for independent seeds.")
    # Finish all validation and timing before publishing either final report.
    # A failed run must not leave a plausible-looking completed-results file.
    timing=metrics(paths,args.warmup)
    analysis=output/"FRESH_SEED_RESULTS"
    analysis.with_suffix(".md").write_text(render(scenes,summary),encoding="utf-8")
    analysis.with_suffix(".json").write_text(
        json.dumps({"aggregate":summary,"scenes":scenes},indent=2)+"\n",
        encoding="utf-8")
    (output/"COMPONENT_TIMING.json").write_text(
        json.dumps(timing,indent=2)+"\n",encoding="utf-8")
    try:
        from .analyze_kyoto_rumble import analyze, write_report
    except ImportError:
        from analyze_kyoto_rumble import analyze, write_report
    rumble=analyze(paths,args.warmup,args.bootstrap,args.sample_seed)
    write_report(rumble,output)
    print("[RUMBLE] overall ranking, seed-blocked intervals, exact-pair audit, "
          "A/A control, and material/regime/size patterns saved",flush=True)
    print("[ABLATION] COMPLETED: new output, original exhaustive data untouched",flush=True)
    print("[ABLATION] scenes:",len(scenes),"fresh seeds:",sorted(args.seeds),flush=True)
    print("[ABLATION] per-trajectory geo Guard/ML:",
          f'{summary["per_trajectory_paired_geometric_mean_ml_vs_global"]:.4f}x',flush=True)
    print("[ABLATION] sum of scene means Guard/ML:",f"{ratio_total:.4f}x",flush=True)
    if "conservative_vs_global_geomean" in summary:
        print("[ABLATION] conservative Guard/ML geomean:",
              f'{summary["conservative_vs_global_geomean"]:.4f}x',flush=True)
        print("[ABLATION] conservative vs default ML geomean:",
              f'{summary["conservative_vs_default_ml_geomean"]:.4f}x',flush=True)
    if "coherent_sap_vs_global_geomean" in summary:
        print("[ABLATION] persistent SAP vs Guard geomean:",
              f'{summary["coherent_sap_vs_global_geomean"]:.4f}x',flush=True)
        print("[ABLATION] existing ML vs persistent SAP geomean:",
              f'{summary["learned_vs_coherent_sap_geomean"]:.4f}x',flush=True)
    if "guard_sap_probe_vs_global_geomean" in summary:
        print("[ABLATION] measured-probe Guard/SAP vs Guard geomean:",
              f'{summary["guard_sap_probe_vs_global_geomean"]:.4f}x',flush=True)
        print("[ABLATION] hybrid probes/switches:",
              summary["guard_sap_probe_count"],summary["guard_sap_probe_switches"],flush=True)
    if "guard_sap_bounded_trial_vs_global_geomean" in summary:
        print("[ABLATION] bounded one-backend trial vs Guard geomean:",
              f'{summary["guard_sap_bounded_trial_vs_global_geomean"]:.4f}x',
              "trial frames:",summary["guard_sap_bounded_trial_trial_frames"],
              "switches:",summary["guard_sap_bounded_trial_switches"],flush=True)
    print("[ABLATION] controller switches:",timing["switches"],flush=True)
    print("[ABLATION] full evidence:",output,flush=True)


if __name__=="__main__":
    main()
