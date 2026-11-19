#!/usr/bin/env python3
"""Inspect an existing Kyoto Phase 17 material run WITHOUT rerunning benchmarks.

The original full-output methods and optional trained ML are examined from
their actual CSV decision fields. Exact full candidate count/hash agreement is
rechecked by the strict score_replay scorer before anything is summarized.

This is a *post hoc* audit, not a new performance experiment or optimizer.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score

V4 = "guard_sap_bounded_trial"
GUARD = "global_guarded_exact"
SAP = "sap_persistent_coherent"
ONLINE = "online_cost_aware"
LEARNED = ("ml_cost_aware", "ml_cost_aware_conservative")
REQUIRED = {V4, GUARD, SAP, ONLINE, "sap_rebuild"}
TRIAL_N_MIN = 384  # Frozen C++ final_broadphase.hpp threshold.


def audit(root: Path, warmup: int) -> dict:
    files = sorted((root / "replays").glob("*-evaluated.csv"))
    if not files:
        raise ValueError(f"No evaluated material replays found under {root / 'replays'}")
    scenes = []
    methods_seen: set[str] = set()
    seeds = set()
    n_values = set()
    totals = {
        "frames": 0, "size_eligible_frames": 0, "guard_frames": 0,
        "sap_frames": 0, "unknown_frames": 0,
        "v4_trial_frames": 0, "v4_switches": 0,
    }
    for path in files:
        # Full sorted pair-set hash and count checked frame-by-frame against SAP.
        scored = score(path, skip=warmup)
        with path.open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            required_columns = {
                "backend", "n", "seed", "frame", "chosen_backend",
                "hybrid_probed", "hybrid_switched",
            }
            if not required_columns.issubset(reader.fieldnames or []):
                raise ValueError(f"Missing decision audit columns: {path}")
            data = list(reader)
        frame_count = scored["methods"][GUARD]["frames_measured"] + warmup
        if not data:
            raise ValueError(f"Empty replay: {path}")
        methods = {row["backend"] for row in data}
        if not REQUIRED.issubset(methods):
            raise ValueError(f"Core policies missing: {path}: {sorted(REQUIRED - methods)}")
        methods_seen.update(methods)
        n = int(data[0]["n"])
        seed = int(data[0]["seed"])
        if n <= 0:
            raise ValueError("Invalid number of AABBs")
        if any(int(row["n"]) != n or int(row["seed"]) != seed for row in data):
            raise ValueError(f"Mixed scene sizes/seeds inside one immutable replay: {path}")
        seeds.add(seed)
        n_values.add(n)
        v4 = sorted((row for row in data if row["backend"] == V4),
                    key=lambda x: int(x["frame"]))
        if len(v4) != frame_count or [int(r["frame"]) for r in v4] != list(range(frame_count)):
            raise ValueError(f"Missing or repeated V4 decision frames: {path}")
        actions = Counter(row["chosen_backend"] for row in v4)
        unexpected = set(actions) - {GUARD, SAP}
        if unexpected:
            raise ValueError(f"V4 emitted unknown backends {sorted(unexpected)} in {path}")
        trial_frames = sum(int(row["hybrid_probed"]) for row in v4)
        switches = sum(int(row["hybrid_switched"]) for row in v4)
        if any(row["hybrid_probed"] not in {"0", "1"} or
               row["hybrid_switched"] not in {"0", "1"} for row in v4):
            raise ValueError(f"V4 decision fields are not Boolean: {path}")
        if n < TRIAL_N_MIN and (actions[SAP] or trial_frames or switches):
            raise ValueError(
                f"V4 returned a SAP trial or switch under frozen size floor: {path}")
        totals["frames"] += frame_count
        totals["size_eligible_frames"] += frame_count if n >= TRIAL_N_MIN else 0
        totals["guard_frames"] += actions[GUARD]
        totals["sap_frames"] += actions[SAP]
        totals["v4_trial_frames"] += trial_frames
        totals["v4_switches"] += switches
        scenes.append({
            "file": str(path), "n": n, "seed": seed,
            "frames": frame_count,
            "v4_size_eligible": n >= TRIAL_N_MIN,
            "v4_backend_frames": dict(actions),
            "v4_trial_frames": trial_frames, "v4_switches": switches,
            "global_guard_mean_ms_excluding_warmup":
                scored["methods"][GUARD]["mean_ms"],
            "v4_mean_ms_excluding_warmup":
                scored["methods"][V4]["mean_ms"],
            "guard_over_v4_excluding_warmup":
                scored["methods"][GUARD]["mean_ms"] /
                scored["methods"][V4]["mean_ms"],
            "online_mean_ms_excluding_warmup":
                scored["methods"][ONLINE]["mean_ms"],
        })
    if totals["frames"] != totals["guard_frames"] + totals["sap_frames"]:
        raise ValueError("Missing V4 current-frame action accounting")
    ratios = [scene["guard_over_v4_excluding_warmup"] for scene in scenes]
    return {
        "source": str(root), "replays": len(scenes),
        "unique_seed_values": sorted(seeds), "n_values": sorted(n_values),
        "actual_methods": sorted(methods_seen),
        "learned_model_executed": any(name in methods_seen for name in LEARNED),
        "bullet_executed": "bullet_dbvt_persistent" in methods_seen,
        "frozen_min_n_for_sap_trial": TRIAL_N_MIN,
        "v4_actions": totals,
        "guard_v4_geomean_from_warm_csv":
            math.exp(sum(math.log(ratio) for ratio in ratios) / len(ratios)),
        "limitations": [
            "N >= 384 is necessary for the V4 SAP trial but not sufficient; "
            "it also requires three expensive Guard rebuilds, EWMA >= 0.35 ms and no cooldown",
            "V4 trial-frame and switch counts are measured states, not "
            "theoretical activation inferred from scene names",
            "Warmup-excluded CSV means differ from all-frame console means",
            "Only one seed is not an independently replicated performance study",
            "Original generalized Online is NOT automatically the same as "
            "the adaptive topology controller inside the physical cloth solver",
            "An absent learned model is NOT evidence the ML algorithm failed",
            "CPU broad-phase candidate parity does not prove CCD or physical fidelity",
        ],
        "scenes": scenes,
    }


def render(result: dict, destination: Path) -> None:
    if destination.exists():
        raise FileExistsError(f"Refusing to replace previous audit: {destination}")
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "V4_DECISION_AUDIT.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    a = result["v4_actions"]
    lines = [
        "# Kyoto Phase 17: actual online and V4 decisions", "",
        "Existing verified full-pair material CSVs only. This command "
        "**does not rerun a simulation or time anything**.", "",
        f"Trajectories: **{result['replays']}**; actual N values: "
        f"**{', '.join(map(str, result['n_values']))}**; seeds: "
        f"**{', '.join(map(str, result['unique_seed_values']))}**.", "",
        f"Frozen V4 minimum N for SAP trial: **{TRIAL_N_MIN}**. "
        f"Size-eligible frames: **{a['size_eligible_frames']} / {a['frames']}**.", "",
        f"V4 Guard frames: **{a['guard_frames']}**. V4 SAP frames: "
        f"**{a['sap_frames']}**. Marked trial frames: "
        f"**{a['v4_trial_frames']}**. Backend switches: "
        f"**{a['v4_switches']}**.", "",
        f"From the *warmup-excluded CSV scorer* (not console means), "
        f"Guard/V4 scene-level geometric mean: "
        f"**{result['guard_v4_geomean_from_warm_csv']:.4f}×**.", "",
        f"Learned controller actually measured: "
        f"**{'YES' if result['learned_model_executed'] else 'NO — model not supplied'}**. "
        f"Bullet DBVT actually measured: "
        f"**{'YES' if result['bullet_executed'] else 'NO — build not linked'}**.", "",
        "| Trajectory | N | V4 Guard frames | V4 SAP frames | Trial frames | Switches | Warm Guard/V4 |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for scene in result["scenes"]:
        counts = scene["v4_backend_frames"]
        lines.append(
            f"| {Path(scene['file']).stem} | {scene['n']} | "
            f"{counts.get(GUARD, 0)} | {counts.get(SAP, 0)} | "
            f"{scene['v4_trial_frames']} | {scene['v4_switches']} | "
            f"{scene['guard_over_v4_excluding_warmup']:.3f}× |")
    lines += ["", "## Interpretation", ""]
    if a["size_eligible_frames"] == 0:
        lines.append(
            "**None of these trajectories could activate V4's bounded SAP "
            "trial**, because every scene has fewer than 384 AABBs. "
            "These results describe the controller's Guard execution "
            "path, not its adaptive switching ability.")
    elif a["sap_frames"] == 0:
        lines.append(
            "**No actual SAP frames were observed**, despite some "
            "size-eligible scenes. Inspect costly Guard rebuild "
            "streaks and measured decision thresholds before interpreting "
            "this as a head-to-head online selector test.")
    else:
        lines.append(
            "A real V4 SAP execution path was exercised; verify "
            "switching overhead and workload coverage before any "
            "performance generalization.")
    lines += ["", "## Qualifications", ""]
    lines.extend("- " + item for item in result["limitations"])
    (destination / "V4_DECISION_AUDIT.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--materials", type=Path, required=True)
    p.add_argument("--warmup", type=int, default=4)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = audit(args.materials, args.warmup)
    render(result, args.output)
    print(f"PASS source-exact V4 control-flow audit: "
          f"{result['replays']} trajectories, "
          f"{result['v4_actions']['sap_frames']} actual SAP frames, "
          f"learned_executed={result['learned_model_executed']}; "
          f"{args.output / 'V4_DECISION_AUDIT.md'}")


if __name__ == "__main__":
    main()
