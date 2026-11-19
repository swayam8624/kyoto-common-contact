#!/usr/bin/env python3
"""Repeated A/A audit of already exported Kyoto Phase 18 material snapshots.

This runs CPU replay timings (NOT simulations or exporters) using frozen
snapshot input files with their original SHA-256 receipts. Each independent
replay has 13 original policies (11 if Bullet unavailable + two clones):
- independent identical Global Guard copies A/B;
- independent identical frozen V4 copies A/B;
- original other policies, on identical immutable full AABB snapshots.

The replayer randomizes policy execution order under a documented seed.
Every method is checked against SAP for each frame, AND the SAP reference
is compared with the original Phase 18 saved replay for that scene.
An A/A contrast is a noise/control-flow diagnostic, not proof of a new win.

Usage:
  python3 scripts/recheck_kyoto_phase18_aa.py \
    --results output/kyoto-decision-mac-first \
    --replayer build/kyoto-final-cpu/ccp_material_replay \
    --output output/kyoto-phase18-aa-mac \
    --repeats 4
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import statistics
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score
sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import check_ordered_replay, sha256

GUARD = "global_guarded_exact"
V4 = "guard_sap_bounded_trial"
GUARD_B = "global_guarded_exact_aa"
V4_B = "guard_sap_bounded_trial_aa"
SAP = "sap_rebuild"
REQUIRED = {GUARD, V4, GUARD_B, V4_B, SAP, "online_cost_aware",
            "sap_persistent_coherent"}
COHORTS = {"natural", "rigid_translation_diagnostic"}


def gmean(values):
    values = list(values)
    if not values or any(not math.isfinite(x) or x <= 0 for x in values):
        raise ValueError("Geometric mean requires positive finite observations")
    return math.exp(sum(math.log(x) for x in values) / len(values))


def percentile_nearest(values, percentile):
    if not values:
        raise ValueError("No A/A observations")
    ordered = sorted(values)
    return ordered[math.ceil(percentile * len(ordered)) - 1]


def sap_signatures(path):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        required = {"backend", "frame", "pairs", "pair_hash"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("Missing exact pair signature columns: " + str(path))
        return {
            int(row["frame"]): (int(row["pairs"]), row["pair_hash"])
            for row in reader if row["backend"] == SAP
        }


def action_frames(path, backend):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        result = [row for row in reader if row["backend"] == backend]
    result.sort(key=lambda row: int(row["frame"]))
    return result


def test_aa(path, original, orderseed, frames, warmup):
    # Strict replay scorer verifies complete framewise pair count/hash parity
    # across both new control clones as well as the eleven historical methods.
    data = score(path, skip=warmup)
    methods = set(data["methods"])
    if not REQUIRED.issubset(methods):
        raise ValueError("A/A replay lacks required independent copies: " + str(path))
    order = check_ordered_replay(path, orderseed)
    if len(order) != len(methods):
        raise ValueError("Execution slots cannot be paired: " + str(path))
    if sap_signatures(path) != original:
        raise ValueError("Repeat changed original SAP candidate truth: " + str(path))
    v4_a = action_frames(path, V4)
    v4_b = action_frames(path, V4_B)
    if len(v4_a) != frames or len(v4_b) != frames:
        raise ValueError("Missing V4 per-frame A/A decisions")
    if [int(x["frame"]) for x in v4_a] != list(range(frames)) or \
       [int(x["frame"]) for x in v4_b] != list(range(frames)):
        raise ValueError("V4 A/A frame sequence corrupt")
    choices = [
        {
            "sap": sum(x["chosen_backend"] == "sap_persistent_coherent" for x in rows),
            "trial": sum(int(x["hybrid_probed"]) for x in rows),
            "switches": sum(int(x["hybrid_switched"]) for x in rows),
        }
        for rows in (v4_a, v4_b)
    ]
    for source in (v4_a, v4_b):
        if any(x["chosen_backend"] not in {
            "global_guarded_exact", "sap_persistent_coherent"
        } for x in source):
            raise ValueError("Unexpected V4 action label")
        if any(x["hybrid_probed"] not in {"0", "1"} or
               x["hybrid_switched"] not in {"0", "1"} for x in source):
            raise ValueError("Invalid V4 decision booleans")
    choice_disagreements = sum(
        a["chosen_backend"] != b["chosen_backend"]
        for a, b in zip(v4_a, v4_b)
    )
    time = {k: data["methods"][k]["mean_ms"] for k in
            (GUARD, GUARD_B, V4, V4_B)}
    return {
        "methods": sorted(methods),
        "order": order,
        "a_b_identities_assigned_before_shuffle": True,
        "mean_ms": time,
        "guard_aa_ratio": time[GUARD] / time[GUARD_B],
        "v4_aa_ratio": time[V4] / time[V4_B],
        "paired_guard_over_v4_a": time[GUARD] / time[V4],
        "paired_guard_over_v4_b": time[GUARD_B] / time[V4_B],
        "v4_a": choices[0], "v4_b": choices[1],
        "v4_action_mismatch_frames": choice_disagreements,
    }


def run_case(source, case, output, repeats, replayer, warmup, timeout):
    snapshot = Path(case["snapshot"])
    old_csv = Path(case["csv"])
    for path, expected in ((snapshot, case["snapshot_sha256"]),
                           (old_csv, case["csv_sha256"])):
        if not path.is_file() or sha256(path) != expected:
            raise ValueError("Missing/mutated prior input: " + str(path))
    original = sap_signatures(old_csv)
    if sorted(original) != list(range(case["frames"])):
        raise ValueError("Original replay has incomplete SAP oracle")
    rows = []
    for repeat in range(repeats):
        # Derive deterministic independent execution order directly from the
        # immutable snapshot SHA and repeat counter. Zero is reserved for
        # unrandomized legacy replays and is never emitted here.
        raw = (case["snapshot_sha256"] + "|KYOTO_AA_V1|" +
               str(repeat)).encode("utf-8")
        orderseed = int.from_bytes(hashlib.sha256(raw).digest()[:4], "little") or 1
        name = (snapshot.stem + "-aa-repeat" + str(repeat + 1))
        new_csv = output / "replays" / (name + "-evaluated.csv")
        log = output / "logs" / (name + ".log")
        cmd = [str(replayer), str(snapshot), str(new_csv),
               "0", "--order-seed", str(orderseed), "--aa-control"]
        with log.open("x", encoding="utf-8") as target:
            target.write("COMMAND: " + " ".join(cmd) + "\n")
            target.flush()
            result = subprocess.run(cmd, stdout=target, stderr=subprocess.STDOUT,
                                    check=False, timeout=timeout)
        if result.returncode != 0:
            raise RuntimeError("A/A replayer returned " + str(result.returncode) +
                               "; inspect " + str(log))
        scored = test_aa(new_csv, original, orderseed, case["frames"], warmup)
        rows.append({
            "type": case["type"], "material": case["material"],
            "regime": case["regime"], "n": case["actual_n"],
            "seed": case["seed"], "repeat": repeat + 1,
            "input_sha256": case["snapshot_sha256"],
            "old_csv_sha256": case["csv_sha256"],
            "new_csv": str(new_csv), "new_csv_sha256": sha256(new_csv),
            "execution_order_seed": orderseed, **scored
        })
        print("PASS A/A", case["type"], case["material"], case["regime"],
              "N=" + str(case["actual_n"]), "phase-seed=" + str(case["seed"]),
              "repeat=" + str(repeat + 1),
              "GuardA/GuardB=" + format(scored["guard_aa_ratio"], ".4f"),
              "V4A/V4B=" + format(scored["v4_aa_ratio"], ".4f"),
              "choices-differ=" + str(scored["v4_action_mismatch_frames"]),
              flush=True)
    return rows


def summarize(rows):
    by_cohort = defaultdict(list)
    for row in rows:
        by_cohort[row["type"]].append(row)
    group = {}
    for cohort, entries in sorted(by_cohort.items()):
        guard = [abs(math.log(x["guard_aa_ratio"])) for x in entries]
        v4 = [abs(math.log(x["v4_aa_ratio"])) for x in entries]
        improvement = [x["paired_guard_over_v4_a"] for x in entries] + [
            x["paired_guard_over_v4_b"] for x in entries]
        group[cohort] = {
            "paired_runs": len(entries),
            "guard_v4_geomean_over_two_independent_copies": gmean(improvement),
            "guard_a_vs_guard_b_geomean": gmean(x["guard_aa_ratio"] for x in entries),
            "v4_a_vs_v4_b_geomean": gmean(x["v4_aa_ratio"] for x in entries),
            "median_guard_aa_absolute_log_ratio": statistics.median(guard),
            "p95_guard_aa_absolute_log_ratio": percentile_nearest(guard, .95),
            "median_v4_aa_absolute_log_ratio": statistics.median(v4),
            "p95_v4_aa_absolute_log_ratio": percentile_nearest(v4, .95),
            "v4_action_mismatched_frames": sum(x["v4_action_mismatch_frames"]
                                               for x in entries),
            "v4_a_sap_frames": sum(x["v4_a"]["sap"] for x in entries),
            "v4_b_sap_frames": sum(x["v4_b"]["sap"] for x in entries),
            "total_guard_vs_v4_pair_comparisons": len(improvement),
            "limitations": "A/A ratios quantify observed same-scene same-code disagreement, NOT a statistical confidence interval"
        }
    conditions = defaultdict(list)
    for x in by_cohort.get("natural", []):
        conditions[(x["material"], x["regime"], x["n"])].append(x)
    condition_summary = []
    for (material, regime, n), entries in sorted(conditions.items()):
        condition_summary.append({
            "material": material, "regime": regime, "n": n,
            "paired_runs": len(entries),
            "phase_seeds": sorted(set(x["seed"] for x in entries)),
            "guard_over_v4_geomean": gmean(
                ratio for x in entries for ratio in (
                    x["paired_guard_over_v4_a"], x["paired_guard_over_v4_b"])),
            "guard_aa_abs_log_median": statistics.median(
                abs(math.log(x["guard_aa_ratio"])) for x in entries),
            "v4_action_mismatched_frames": sum(
                x["v4_action_mismatch_frames"] for x in entries)
        })
    return group, condition_summary


def save(data, output):
    if (output / "AA_RECHECK.json").exists():
        raise FileExistsError("Refusing replacement of completed A/A report")
    (output / "AA_RECHECK.json").write_text(
        json.dumps(data, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Kyoto Phase 18 — same-snapshot randomized A/A evidence", "",
        "**The original material simulations were NOT rerun.** Existing original"
        " KYOTOM01 snapshot SHA-256 checks and original/current SAP candidate"
        " fingerprints were enforced; only the CPU query timings were repeated.",
        "",
        "A/B identities were assigned BEFORE execution-order randomization," 
        " so either copy may execute first in each repeated timing run.",
        "",
        "Two independent instances of the SAME Global Guard and frozen V4"
        " implementations are measured on each snapshot stream. Duplicates"
        " preserve independent caches and decision state and never share"
        " chosen outcomes.",
        "",
        "This is an **exploratory noise-floor and controller-stability study**,"
        " not an automatic publication-level statistical significance test.",
        ""
    ]
    for cohort, x in data["cohorts"].items():
        label = ("Naturally stepped materials" if cohort == "natural" else
                 "ARTIFICIAL rigid whole-scene translation (diagnostic)")
        lines.extend([
            "## " + label, "",
            "Repeated randomized replay runs: **" + str(x["paired_runs"]) + "**.",
            "Guard/V4 geometric mean across both copies: **" +
                format(x["guard_v4_geomean_over_two_independent_copies"], ".4f") + "x**.",
            "Guard A/A geomean A/B: **" +
                format(x["guard_a_vs_guard_b_geomean"], ".4f") + "x**.",
            "V4 A/A geomean A/B: **" +
                format(x["v4_a_vs_v4_b_geomean"], ".4f") + "x**.",
            "Median absolute Guard A/A log ratio: **" +
                format(x["median_guard_aa_absolute_log_ratio"], ".5f") + "**.",
            "95th percentile of absolute Guard A/A log ratio: **" +
                format(x["p95_guard_aa_absolute_log_ratio"], ".5f") + "**.",
            "V4 A/B disagreeing backend frames: **" +
                str(x["v4_action_mismatched_frames"]) + "**.",
            "V4 A/B SAP action frame totals: **" +
                str(x["v4_a_sap_frames"]) + "/" + str(x["v4_b_sap_frames"]) + "**.",
            ""
        ])
    lines += ["## Naturally generated condition breakdown", "",
              "| Material | Regime | N | Repeated runs | Guard/V4 geometric mean | Guard A/A median abs(log) | V4 decision disagreement frames |",
              "|---|---|---:|---:|---:|---:|---:|"]
    for x in data["natural_conditions"]:
        lines.append("| " + x["material"] + " | " + x["regime"] + " | " +
                     str(x["n"]) + " | " + str(x["paired_runs"]) + " | " +
                     format(x["guard_over_v4_geomean"], ".4f") + "x | " +
                     format(x["guard_aa_abs_log_median"], ".5f") + " | " +
                     str(x["v4_action_mismatched_frames"]) + " |")
    lines += ["", "## Essential boundaries", "",
        "- Identical A/A method pairs can diverge in timing or timing-triggered"
          " V4 decisions due to clock/cache/thermal perturbations.",
        "- Policy methods execute entire consecutive trajectories in different"
          " randomized order slots. This is NOT a per-frame interleaved"
          " ABBA design and cannot cancel all temporal drift.",
        "- Same material and topology reused across forcing-phase seeds;"
          " not independent physical initial conditions.",
        "- Exactness was assessed by candidate count + ordered-pair hash"
          " equality plus the native small-scene exhaustive tests, not"
          " stored full pair-ID files in every replay.",
        "- No contact solver, physical fidelity or CCD timing measured.",
        "- Results are not a model-training set or a license to retune V4"
          " thresholds using the same evaluation data.",
    ]
    (output / "AA_RECHECK.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--results", required=True, type=Path)
    p.add_argument("--replayer", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--repeats", type=int, default=4)
    p.add_argument("--materials", nargs="+", default=None)
    p.add_argument("--regimes", nargs="+", default=None)
    p.add_argument("--min-n", type=int, default=0)
    p.add_argument("--include-diagnostic", action="store_true")
    p.add_argument("--timeout-seconds", type=int, default=600)
    a = p.parse_args()
    if a.output.exists():
        p.error("Refusing to overwrite A/A output: " + str(a.output))
    if not 2 <= a.repeats <= 12:
        p.error("A/A repeats must be 2..12")
    if not 0 <= a.min_n <= 50000:
        p.error("Invalid --min-n")
    replayer = a.replayer.resolve()
    if not replayer.is_file():
        p.error("Native C++ replayer binary not found: " + str(replayer))
    capabilities = subprocess.run(
        [str(replayer), "--capabilities"], capture_output=True, text=True,
        check=False, timeout=20)
    if capabilities.returncode or "aa_labels_shuffled_independently" not in capabilities.stdout:
        p.error("Stale CPU binary: build latest ccp_material_replay first")
    manifest = a.results / "DECISION_COVERAGE.json"
    original = json.loads(manifest.read_text(encoding="utf-8"))
    if original.get("experiment") != \
            "Phase 18 controller-activation coverage, no threshold retuning":
        p.error("Unrecognized Phase 18 source manifest")
    if original.get("natural_and_artificial_diagnostic_never_pooled") is not True:
        p.error("Source mixed natural and synthetic controls")
    if not 0 <= original["warmup"] < min(c["frames"] for c in original["data"]):
        p.error("Original warmup invalid")
    chosen = [
        c for c in original["data"]
        if (c["type"] == "natural" or a.include_diagnostic) and
           (a.materials is None or c["material"] in a.materials) and
           (a.regimes is None or c["regime"] in a.regimes) and
           c["actual_n"] >= a.min_n
    ]
    if not chosen:
        p.error("No original immutable snapshots match the selection")
    if len({c["csv"] for c in chosen}) != len(chosen):
        p.error("Duplicate source snapshots")
    a.output.mkdir(parents=True, exist_ok=False)
    (a.output / "replays").mkdir()
    (a.output / "logs").mkdir()
    all_rows = []
    for case in chosen:
        if case["type"] not in COHORTS:
            raise ValueError("Unexpected source cohort")
        all_rows += run_case(original, case, a.output, a.repeats,
                             replayer, int(original["warmup"]),
                             a.timeout_seconds)
    cohorts, conditions = summarize(all_rows)
    summary = {
        "experiment": "Phase 18 replicated same-snapshot native A/A",
        "a_b_identities_assigned_before_shuffle": True,
        "source_manifest": str(manifest), "source_manifest_sha256": sha256(manifest),
        "source_trajectories": len(chosen), "repeats_per_trajectory": a.repeats,
        "warmup_excluded": int(original["warmup"]),
        "performed_no_new_material_simulations": True,
        "timed_native_replays": len(all_rows),
        "cohorts": cohorts, "natural_conditions": conditions,
        "per_repeat": all_rows
    }
    save(summary, a.output)
    print("PASS: A/A repeated raw policy timings verified under " +
          str(len(all_rows)) + " randomized orders, without new simulations",
          flush=True)
    print(a.output / "AA_RECHECK.md", flush=True)


if __name__ == "__main__":
    main()
