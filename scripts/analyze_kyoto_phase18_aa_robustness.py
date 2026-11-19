#!/usr/bin/env python3
"""Post-run robustness of completed Kyoto frozen-Guard vs V4 A/A measurements.

Reads ONLY existing AA_RECHECK.json plus its hashed replay CSVs. Does not
invoke C++, re-run timing, export scenes, or train anything. All calculations
operate on paired per-trajectory repeat logs; adjacent frames are NOT treated
as independent observations.

Bootstrap below is explicitly a *conditional timing-repeat resampling
envelope for this fixed suite*, NOT a population confidence interval.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import random
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, median

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score

GUARD_A = "global_guarded_exact"
GUARD_B = "global_guarded_exact_aa"
V4_A = "guard_sap_bounded_trial"
V4_B = "guard_sap_bounded_trial_aa"
METHODS = (GUARD_A, GUARD_B, V4_A, V4_B)


def sha256(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1 << 20), b""):
            result.update(chunk)
    return result.hexdigest()


def finite_positive(value) -> float:
    x = float(value)
    if not math.isfinite(x) or x <= 0:
        raise ValueError("Nonfinite or nonpositive ratio/timing")
    return x


def geom_from_logs(logs) -> float:
    values = list(logs)
    if not values:
        raise ValueError("No log observations")
    return math.exp(mean(values))


def q_nearest(sorted_values, p):
    return sorted_values[max(0, math.ceil(p * len(sorted_values)) - 1)]


def close_to(a, b):
    return math.isclose(float(a), float(b), rel_tol=2e-7, abs_tol=2e-7)


def validate_record(row: dict, warmup: int) -> dict:
    source = Path(row["new_csv"])
    if not source.is_file() or sha256(source) != row["new_csv_sha256"]:
        raise ValueError("CSV missing or SHA-256 mismatch: " + str(source))
    if row["type"] not in {"natural", "rigid_translation_diagnostic"}:
        raise ValueError("Unknown cohort classification in " + str(source))
    actual = score(source, skip=warmup)
    if set(actual["methods"]) != set(row["methods"]):
        raise ValueError("Missing algorithm in repeated replay " + str(source))
    observed = actual["methods"]
    for method in METHODS:
        measured = observed[method]["mean_ms"]
        if not close_to(measured, row["mean_ms"][method]):
            raise ValueError("Stored native mean changed for " + method)
    # A vital historical confound: the first A/A replayer assigned
    # copy A/B identity AFTER policy shuffling. Therefore A was always
    # earlier than B, and an A/B mean ratio may contain temporal drift.
    # New versions assign identities BEFORE randomizing the whole schedule.
    order = row["order"]
    slots = [int(order[m]) for m in METHODS]
    if len(set(slots)) != 4:
        raise ValueError("A/A controls did not occupy distinct execution slots")
    guard_a_earlier = int(order[GUARD_A]) < int(order[GUARD_B])
    v4_a_earlier = int(order[V4_A]) < int(order[V4_B])
    values = {
        "ga": finite_positive(row["mean_ms"][GUARD_A]),
        "gb": finite_positive(row["mean_ms"][GUARD_B]),
        "va": finite_positive(row["mean_ms"][V4_A]),
        "vb": finite_positive(row["mean_ms"][V4_B]),
    }
    # Cross-check timing-triggered V4 action instability frame-by-frame,
    # rather than merely trusting the compact A/A JSON summary. These rows
    # are DESCRIPTIVE evidence, not a new exact-pair oracle.
    v4_decisions = {V4_A: {}, V4_B: {}}
    with source.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        for item in reader:
            if item["backend"] in v4_decisions:
                method_rows = v4_decisions[item["backend"]]
                frame = int(item["frame"])
                if frame in method_rows:
                    raise ValueError("Repeated V4 frame row: " + str(source))
                method_rows[frame] = {
                    "selected": item["chosen_backend"],
                    "trial": int(item["hybrid_probed"]),
                    "switch": int(item["hybrid_switched"]),
                    "measured_ms": finite_positive(item["ms"]),
                }
    frames_a = v4_decisions[V4_A]
    frames_b = v4_decisions[V4_B]
    if not frames_a or set(frames_a) != set(frames_b) or        set(frames_a) != set(range(len(frames_a))):
        raise ValueError("V4 clones do not cover the same consecutive frames")
    frame_divergences = [
        {"frame": index, "before_warmup": index < warmup,
         "v4_A": frames_a[index], "v4_B": frames_b[index]}
        for index in sorted(frames_a)
        if frames_a[index]["selected"] != frames_b[index]["selected"]
    ]
    if len(frame_divergences) != int(row["v4_action_mismatch_frames"]):
        raise ValueError("Recorded V4 disagreement count differs from CSV")
    for backend, expected in ((V4_A, row["v4_a"]), (V4_B, row["v4_b"])):
        frames = v4_decisions[backend].values()
        if (sum(x["selected"] == "sap_persistent_coherent" for x in frames)
                != int(expected["sap"])) or            sum(x["trial"] for x in frames) != int(expected["trial"]) or            sum(x["switch"] for x in frames) != int(expected["switches"]):
            raise ValueError("V4 action receipt differs from full replay CSV")
    ratios = {
        "guard_aa_ratio": values["ga"] / values["gb"],
        "v4_aa_ratio": values["va"] / values["vb"],
        "paired_guard_over_v4_a": values["ga"] / values["va"],
        "paired_guard_over_v4_b": values["gb"] / values["vb"],
    }
    if any(not close_to(row[key], observed_ratio)
           for key, observed_ratio in ratios.items()):
        raise ValueError("A/A or Guard/V4 stored ratio disagrees with raw timing")
    if int(row["repeat"]) <= 0 or int(row["seed"]) <= 0 or int(row["n"]) <= 0:
        raise ValueError("Invalid repeated scene label")
    if int(row["v4_action_mismatch_frames"]) < 0:
        raise ValueError("Negative V4 action divergence")
    return {
        "type": row["type"], "material": row["material"],
        "regime": row["regime"], "n": int(row["n"]),
        "guard_a_earlier": guard_a_earlier,
        "v4_a_earlier": v4_a_earlier,
        "seed": int(row["seed"]), "repeat": int(row["repeat"]),
        "log_guard_v4": .5 * (math.log(ratios["paired_guard_over_v4_a"]) +
                              math.log(ratios["paired_guard_over_v4_b"])),
        "log_guard_aa": math.log(ratios["guard_aa_ratio"]),
        "log_v4_aa": math.log(ratios["v4_aa_ratio"]),
        "actions_mismatched": int(row["v4_action_mismatch_frames"]),
        "frame_divergences": frame_divergences,
        "v4_a_sap_frames": int(row["v4_a"]["sap"]),
        "v4_b_sap_frames": int(row["v4_b"]["sap"]),
        "v4_a_trial_frames": int(row["v4_a"]["trial"]),
        "v4_b_trial_frames": int(row["v4_b"]["trial"]),
        "source": str(source),
    }


def grouped_natural(rows):
    result = defaultdict(list)
    for row in rows:
        if row["type"] == "natural":
            key = (row["material"], row["regime"], row["n"])
            result[key].append(row)
    return dict(sorted(result.items()))


def condition_log(rows):
    return mean(r["log_guard_v4"] for r in rows)


def balanced_log(conditions):
    if not conditions:
        raise ValueError("No naturally stepped trajectories")
    return mean(condition_log(rows) for rows in conditions.values())


def conditional_resampling(conditions, draws: int, seed: int, exclude=None):
    """Resample *complete method copies in a repeat*, never individual frames.

    Every saved trajectory/phase seed has the same weight within its
    condition, and every condition has the same weight in the suite.
    Repeatedly draw its observed time-repeat log contrasts, preserving
    Guard/V4 A/B pairing and frozen scene identities.
    """
    rng = random.Random(seed)
    eligible = {key: values for key, values in conditions.items()
                if key != exclude}
    by_condition = {}
    for key, values in eligible.items():
        phases = defaultdict(list)
        for value in values:
            phases[value["seed"]].append(value["log_guard_v4"])
        by_condition[key] = {
            phase: tuple(samples) for phase, samples in phases.items()
        }
    estimates = []
    for _ in range(draws):
        condition_means = []
        for phases in by_condition.values():
            phase_means = []
            for values in phases.values():
                # Entire repeated method-order runs are sampled as units.
                chosen = [values[rng.randrange(len(values))]
                          for _ in range(len(values))]
                phase_means.append(mean(chosen))
            condition_means.append(mean(phase_means))
        estimates.append(mean(condition_means))
    estimates.sort()
    return {
        "draws": draws, "lower_2_5_percent_speedup": math.exp(q_nearest(estimates, .025)),
        "median_speedup": math.exp(q_nearest(estimates, .5)),
        "upper_97_5_percent_speedup": math.exp(q_nearest(estimates, .975)),
        "fraction_of_resamples_above_1": sum(x > 0 for x in estimates) / draws,
        "interpretation": (
            "Timing-repeat resampling envelope CONDITIONAL on all fixed "
            "material geometries, three forcing phases, and measured order "
            "seeds. Not a statistical CI or probability of new-scene superiority."
        )
    }


def analyze(aa: Path, draws: int, rng_seed: int) -> dict:
    raw = aa.read_bytes()
    doc = json.loads(raw)
    if doc.get("experiment") != "Phase 18 replicated same-snapshot native A/A":
        raise ValueError("Unsupported A/A source report")
    if doc.get("performed_no_new_material_simulations") is not True:
        raise ValueError("Expected original immutable scene replays")
    records = doc["per_repeat"]
    if len(records) != int(doc["timed_native_replays"]):
        raise ValueError("Missing repeated replay entries")
    if len({r["new_csv"] for r in records}) != len(records):
        raise ValueError("Repeated source CSV path")
    warmup = int(doc["warmup_excluded"])
    parsed = [validate_record(r, warmup) for r in records]
    if len(parsed) != len(records):
        raise ValueError("Failed source validation")
    by_cohort = defaultdict(list)
    for r in parsed:
        by_cohort[r["type"]].append(r)
    for name, values in by_cohort.items():
        given = doc["cohorts"][name]
        if len(values) != given["paired_runs"]:
            raise ValueError("Cohort A/A source count mismatch")
        observed = geom_from_logs(r["log_guard_v4"] for r in values)
        if not close_to(observed, given["guard_v4_geomean_over_two_independent_copies"]):
            raise ValueError("A/A aggregate ratio changed: " + name)
        if sum(x["actions_mismatched"] for x in values) != given["v4_action_mismatched_frames"]:
            raise ValueError("A/A action mismatch total changed: " + name)
    conditions = grouped_natural(parsed)
    if not conditions:
        raise ValueError("No natural trajectory observations")
    reps = defaultdict(set)
    for row in by_cohort["natural"]:
        stem = (row["material"], row["regime"], row["n"], row["seed"])
        if row["repeat"] in reps[stem]:
            raise ValueError("Duplicate repeat/phase identity")
        reps[stem].add(row["repeat"])
    for key, observed_repeats in reps.items():
        if observed_repeats != set(range(1, len(observed_repeats) + 1)):
            raise ValueError("Non-contiguous repeat counter: " + repr(key))
    per_condition = []
    for key, values in conditions.items():
        ratio = math.exp(condition_log(values))
        per_condition.append({
            "material": key[0], "regime": key[1], "n": key[2],
            "repeats": len(values), "phase_seed_count": len({r["seed"] for r in values}),
            "guard_over_v4_geomean": ratio,
            "aa_mismatch_frames": sum(r["actions_mismatched"] for r in values),
            "two_v4_copies_sap_frames": sum(
                r["v4_a_sap_frames"] + r["v4_b_sap_frames"] for r in values),
            "timing_repeat_envelope": conditional_resampling(
                {key: values}, draws, rng_seed),
        })
    overall = math.exp(balanced_log(conditions))
    from_first = doc["cohorts"]["natural"]["guard_v4_geomean_over_two_independent_copies"]
    if len({len(v) for v in conditions.values()}) == 1 and not close_to(overall, from_first):
        raise ValueError("Balanced natural cohort ratio differs from archived aggregate")
    abs_guard = sorted(abs(r["log_guard_aa"]) for r in by_cohort["natural"])
    abs_v4 = sorted(abs(r["log_v4_aa"]) for r in by_cohort["natural"])
    leave_one_out = []
    for key, values in conditions.items():
        remaining = {k: v for k, v in conditions.items() if k != key}
        if remaining:
            leave_one_out.append({
                "removed_material": key[0], "removed_regime": key[1],
                "removed_n": key[2],
                "remaining_conditions": len(remaining),
                "guard_over_v4_geomean": math.exp(balanced_log(remaining)),
            })
    strongest = max(conditions, key=lambda key: condition_log(conditions[key]))
    rest = {key: rows for key, rows in conditions.items() if key != strongest}
    main_stats = {
        "guard_over_v4_geomean": overall,
        "guard_A_executed_before_B": sum(
            x["guard_a_earlier"] for x in by_cohort["natural"]),
        "guard_B_executed_before_A": sum(
            not x["guard_a_earlier"] for x in by_cohort["natural"]),
        "v4_A_executed_before_B": sum(
            x["v4_a_earlier"] for x in by_cohort["natural"]),
        "v4_B_executed_before_A": sum(
            not x["v4_a_earlier"] for x in by_cohort["natural"]),
        "total_natural_repeat_runs": len(by_cohort["natural"]),
        "guard_aa_geomean": geom_from_logs(
            r["log_guard_aa"] for r in by_cohort["natural"]),
        "v4_aa_geomean": geom_from_logs(
            r["log_v4_aa"] for r in by_cohort["natural"]),
        "guard_aa_abs_log_median": median(abs_guard),
        "guard_aa_abs_log_p95": q_nearest(abs_guard, .95),
        "v4_aa_abs_log_median": median(abs_v4),
        "v4_aa_abs_log_p95": q_nearest(abs_v4, .95),
        "guard_aa_abs_ratio_p95": math.exp(q_nearest(abs_guard, .95)),
        "v4_action_mismatch_frames": sum(
            r["actions_mismatched"] for r in by_cohort["natural"]),
        "timing_repeat_envelope": conditional_resampling(
            conditions, draws, rng_seed),
    }
    main_stats["leave_strongest_out"] = {
        "removed_material": strongest[0],
        "removed_regime": strongest[1], "removed_n": strongest[2],
        "remaining_guard_over_v4_geomean": math.exp(balanced_log(rest))
        if rest else None,
        "timing_repeat_envelope": conditional_resampling(
            rest, draws, rng_seed) if rest else None,
    }
    differences = [
        {
            "material": r["material"], "regime": r["regime"], "n": r["n"],
            "seed": r["seed"], "repeat": r["repeat"],
            "mismatch_frames": r["actions_mismatched"],
            "frame_divergences": r["frame_divergences"],
            "v4_a_sap": r["v4_a_sap_frames"],
            "v4_b_sap": r["v4_b_sap_frames"],
            "guard_aa_ratio": math.exp(r["log_guard_aa"]),
            "v4_aa_ratio": math.exp(r["log_v4_aa"]),
        }
        for r in by_cohort["natural"] if r["actions_mismatched"] > 0
    ]
    return {
        "experiment": "Post-hoc Phase 18 A/A robustness, no timings rerun",
        "a_b_identities_assigned_before_shuffle": bool(
            doc.get("a_b_identities_assigned_before_shuffle", False)),
        "source_json": str(aa), "source_json_sha256": hashlib.sha256(raw).hexdigest(),
        "native_replays_executed_by_this_script": 0,
        "recorded_replays_revalidated_by_sap": len(parsed),
        "source_provenance": {
            "original_scene_manifest": doc["source_manifest"],
            "original_scene_manifest_sha256": doc["source_manifest_sha256"],
            "repeats_per_saved_trajectory": doc["repeats_per_trajectory"],
            "saved_trajectory_count": doc["source_trajectories"],
        },
        "natural": main_stats,
        "conditions": per_condition,
        "leave_one_condition_out": leave_one_out,
        "backend_decision_disagreements": differences,
        "limitations": [
            "Resampling intervals are conditional on already observed execution-order repeats, not population confidence intervals.",
            "Three phase-seed values do not independently randomize material geometry or topology.",
            "Four method-order repetitions are not independent physical experiments.",
            "The original 144-run Mac report labeled copy A as the first timed clone AFTER shuffling, so A always preceded B. This is a documented label-order bias, not balanced randomized A/B assignment.",
            "New source reports explicitly mark whether A/B identities were assigned BEFORE shuffling, which is needed for balanced order estimates.",
            "Whole-trajectory randomized scheduling may retain CPU heat/clock order effects.",
            "Per-replay A/B clones share hardware and timing environment; treat them as paired, not independent.",
            "V4 decision rules can react to clock variation; disagreement is not pair-correctness failure.",
            "Only current snapshot AABB candidate sets are checked, not CCD, solver, material fidelity or folding.",
            "Removing a high-performing condition is a sensitivity calculation, not a claim about untested populations.",
        ]
    }


def save(report, destination):
    if destination.exists():
        raise FileExistsError("Refusing to replace evidence: " + str(destination))
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "AA_ROBUSTNESS.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    n = report["natural"]
    main = n["leave_strongest_out"]
    fmt = lambda v: format(v, ".4f") + "x"
    env = lambda v: (fmt(v["lower_2_5_percent_speedup"]) + " .. " +
                     fmt(v["upper_97_5_percent_speedup"]))
    lines = [
        "# Frozen V4 vs Guard — same-Mac A/A robustness from immutable replay CSVs",
        "",
        "Re-analysis ONLY. Native query timings were NOT rerun.",
        "All input replay CSV SHA-256 receipts rechecked; strict framewise "
        "candidate count/hash parity rechecked against SAP.",
        "",
        "## A/B execution-order audit",
        "",
        "Source has fixed A/B identities before shuffling: **" +
          str(report["a_b_identities_assigned_before_shuffle"]) + "**.",
        "Guard A earlier / B earlier: " +
          str(n["guard_A_executed_before_B"]) + " / " +
          str(n["guard_B_executed_before_A"]) + ".",
        "V4 A earlier / B earlier: " +
          str(n["v4_A_executed_before_B"]) + " / " +
          str(n["v4_B_executed_before_A"]) + ".",
        "WARNING: If A always ran earlier, A/A ratios reflect a fixed "
        "label-order confound; they remain useful as descriptive timing "
        "disagreement but do NOT provide a balanced A/B noise estimate.",
        "",
        "## Fixed natural suite",
        "",
        "Guard/V4 ratio from equal-weighted conditions: **" +
          fmt(n["guard_over_v4_geomean"]) + "**.",
        "Conditional timing-repeat resampling envelope: **" +
          env(n["timing_repeat_envelope"]) + "**.",
        "",
        "A/A Guard median absolute log ratio: " +
          format(n["guard_aa_abs_log_median"], ".5f") +
          "; nearest-rank p95: " +
          format(n["guard_aa_abs_log_p95"], ".5f") + ".",
        "A/A V4 median absolute log ratio: " +
          format(n["v4_aa_abs_log_median"], ".5f") +
          "; nearest-rank p95: " +
          format(n["v4_aa_abs_log_p95"], ".5f") + ".",
        "Total V4 A/B selected-backend disagreement frames: " +
          str(n["v4_action_mismatch_frames"]) + ".", "",
        "## Sensitivity to the strongest condition",
        "",
        "Strongest condition: " + main["removed_material"] + "/" +
          main["removed_regime"] + "/N=" + str(main["removed_n"]) + ".",
        "Guard/V4 after removing this one condition: **" +
          fmt(main["remaining_guard_over_v4_geomean"]) + "**.",
        "Conditional timing-repeat resampling envelope for the remaining "
        "fixed conditions: **" + env(main["timing_repeat_envelope"]) + "**.",
        "",
        "## Per-condition fixed-snapshot analyses",
        "",
        "| Material | Regime | Actual N | Repeat runs | Guard/V4 | Conditional timing-repeat envelope | A/B mismatch frames |",
        "|---|---|---:|---:|---:|---|---:|"
    ]
    for c in report["conditions"]:
        lines.append("| " + c["material"] + " | " + c["regime"] +
                     " | " + str(c["n"]) + " | " + str(c["repeats"]) +
                     " | " + fmt(c["guard_over_v4_geomean"]) +
                     " | " + env(c["timing_repeat_envelope"]) +
                     " | " + str(c["aa_mismatch_frames"]) + " |")
    lines.extend(["", "## Actual V4 action disagreements", ""])
    if not report["backend_decision_disagreements"]:
        lines.append("No Guard/SAP choice disagreement observed between V4 clones.")
    else:
        lines.extend([
            "| Material | Regime | N | Phase seed | Repeat | Mismatched frames | V4 A SAP | V4 B SAP |",
            "|---|---|---:|---:|---:|---:|---:|---:|"
        ])
        for d in report["backend_decision_disagreements"]:
            lines.append("| " + d["material"] + " | " + d["regime"] +
                         " | " + str(d["n"]) + " | " + str(d["seed"]) +
                         " | " + str(d["repeat"]) + " | " +
                         str(d["mismatch_frames"]) + " | " +
                         str(d["v4_a_sap"]) + " | " +
                         str(d["v4_b_sap"]) + " |")
        lines += ["", "### Exact differing frame states from original native CSV", "",
                  "| Material | Regime | N | Phase seed | Repeat | Frame | Warmup? | V4 A backend | V4 B backend | V4 A ms | V4 B ms |",
                  "|---|---|---:|---:|---:|---:|---|---|---|---:|---:|"]
        for d in report["backend_decision_disagreements"]:
            for frame in d["frame_divergences"]:
                a, b = frame["v4_A"], frame["v4_B"]
                lines.append("| " + d["material"] + " | " + d["regime"] +
                             " | " + str(d["n"]) + " | " + str(d["seed"]) +
                             " | " + str(d["repeat"]) + " | " +
                             str(frame["frame"]) + " | " +
                             str(frame["before_warmup"]) + " | " +
                             a["selected"] + " | " + b["selected"] +
                             " | " + format(a["measured_ms"], ".6f") +
                             " | " + format(b["measured_ms"], ".6f") + " |")
    lines += ["", "## Methodological boundaries", ""]
    lines += ["- " + v for v in report["limitations"]]
    (destination / "AA_ROBUSTNESS.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--aa", type=Path, required=True,
                   help="Completed AA_RECHECK.json (not original Phase 18 manifest)")
    p.add_argument("--output", type=Path, required=True,
                   help="New directory: existing evidence is never overwritten")
    p.add_argument("--draws", type=int, default=2000)
    p.add_argument("--resampling-seed", type=int, default=20261119)
    args = p.parse_args()
    if not 100 <= args.draws <= 20000:
        p.error("--draws must be 100..20000")
    report = analyze(args.aa, args.draws, args.resampling_seed)
    save(report, args.output)
    n = report["natural"]
    print("PASS existing A/A: " +
          str(report["recorded_replays_revalidated_by_sap"]) +
          " hashed native replays rechecked; 0 new simulations or timings",
          flush=True)
    print("Natural Guard/V4 geomean: " +
          format(n["guard_over_v4_geomean"], ".4f") + "x")
    print("Excluding strongest condition: " +
          format(n["leave_strongest_out"]["remaining_guard_over_v4_geomean"],
                 ".4f") + "x")
    print(args.output / "AA_ROBUSTNESS.md")


if __name__ == "__main__":
    main()
