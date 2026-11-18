#!/usr/bin/env python3
"""One auditable report showing the *separate* Kyoto algorithm families.

Material replay: real full candidate lists, SAP checked on every frame,
Global Guard as the baseline. Synthetic Phase 16: count+fingerprint only,
single-thread flat grid as its baseline. Never compare runtimes across these
two workload families or present a per-frame hindsight winner as deployable.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score  # Reuse the strict full pair-count/hash oracle.

REQUIRED = {
    "sap_rebuild", "sap_persistent_coherent", "global_guarded_exact",
    "selective_guard_exact", "online_cost_aware",
    "guard_sap_bounded_trial", "region_fixed", "region_adaptive",
    "region_hierarchical",
}
FINGERPRINT = ("exact_pairs", "hash64_sum", "hash64_xor", "hash32_sum", "hash32_xor")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def material_results(root: Path, skip: int) -> dict:
    files = sorted(root.glob("replays/*-evaluated.csv"))
    if not files:
        raise ValueError(f"No material replay CSVs under {root / 'replays'}")
    per_method = defaultdict(list)
    sources = []
    method_cohort = None
    for file in files:
        result = score(file, skip)
        methods = set(result["methods"])
        if not REQUIRED.issubset(methods):
            raise ValueError(f"Missing historical comparison algorithms in {file}: {sorted(REQUIRED-methods)}")
        if method_cohort is None:
            method_cohort = methods
        if methods != method_cohort:
            raise ValueError("Do not silently rank algorithms that ran on different material cohorts")
        guard = result["methods"]["global_guarded_exact"]["mean_ms"]
        for name, timing in result["methods"].items():
            per_method[name].append({
                "cohort": file.name, "mean_ms": timing["mean_ms"],
                "p95_ms": timing["p95_ms"],
                "guard_speedup": guard / timing["mean_ms"],
                "frames": timing["frames_measured"],
            })
        sources.append({"path": str(file), "sha256": sha256(file)})
    methods = {}
    for name, values in sorted(per_method.items()):
        speedups = [entry["guard_speedup"] for entry in values]
        methods[name] = {
            "trajectories": len(values),
            "guard_speedup_geomean": math.exp(mean(math.log(x) for x in speedups)),
            "trajectory_wins_vs_guard": sum(x > 1 for x in speedups),
            "sum_of_scene_mean_ms": sum(item["mean_ms"] for item in values),
            "per_trajectory": values,
        }
    guard_sum = methods["global_guarded_exact"]["sum_of_scene_mean_ms"]
    for method in methods.values():
        method["summed_scene_mean_speedup_vs_guard"] = (
            guard_sum / method["sum_of_scene_mean_ms"])
    return {
        "scope": "full sorted AABB pair list on identical material snapshots",
        "reference": "global_guarded_exact",
        "oracle": "sap_rebuild framewise pair count+hash, plus sampled brute oracle where supported",
        "files": sources, "trajectories": len(files), "warmup": skip,
        "methods": methods,
        "warnings": [
            "One execution per method/scene cannot estimate timing uncertainty",
            "Trajectories, not adjacent frames, are the independent aggregation units",
            "All matching broad phases should yield the same physics in an unchanged solver",
            "ML baselines appear only if model weights were explicitly supplied",
            "Bullet DBVT appears only in builds linked with Bullet",
        ],
    }


def csvrows(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise ValueError(f"Empty Phase 16 evidence: {path}")
    return rows


def synthetic_results(root: Path) -> dict | None:
    files = sorted(root.glob("*-threads*.csv"))
    if not files:
        return None
    groups = defaultdict(dict)
    manifests = []
    for file in files:
        match = re.fullmatch(r"(.+)-threads(1|2|4|8|16|32)\.csv", file.name)
        if not match:
            raise ValueError(f"Unsupported thread-count evidence filename: {file.name}")
        scene, thread = match.group(1), int(match.group(2))
        if thread in groups[scene]:
            raise ValueError(f"Duplicate Phase 16 cohort: {scene} {thread}")
        records = csvrows(file)
        if any(row["status"] != "complete" for row in records):
            raise ValueError(f"Incomplete Phase 16 query in {file}")
        if any(int(row["thread_limit"]) != thread for row in records):
            raise ValueError("Thread labels disagree with actual CSV request")
        groups[scene][thread] = records
        manifests.append({"path": str(file), "sha256": sha256(file)})
    summary = {}
    for scene, cohort in sorted(groups.items()):
        if 1 not in cohort:
            raise ValueError(f"Missing serial baseline in {scene}")
        reference = cohort[1]
        for threads, rows in cohort.items():
            if len(rows) != len(reference):
                raise ValueError(f"Frame-count mismatch in {scene} threads={threads}")
            for frame, (r, s) in enumerate(zip(rows, reference)):
                if int(r["frame"]) != frame or int(s["frame"]) != frame:
                    raise ValueError("Phase 16 frames not contiguous/aligned")
                for key in ("n", "seed", "scene", *FINGERPRINT):
                    if r[key] != s[key]:
                        raise ValueError(f"Phase 16 count/hash or geometry mismatch: {scene} frame={frame} {key}")
        warm = 1 if len(reference) > 1 else 0
        baseline = mean(float(row["complete_query_ms"]) for row in reference[warm:])
        times = {}
        for threads, rows in sorted(cohort.items()):
            complete = mean(float(row["complete_query_ms"]) for row in rows[warm:])
            query = mean(float(row["query_ms"]) for row in rows[warm:])
            if complete <= 0 or query <= 0:
                raise ValueError("Nonpositive Phase 16 timings")
            times[str(threads)] = {
                "warm_full_ms": complete,
                "warm_enumeration_ms": query,
                "speedup_vs_same_scene_1thread": baseline / complete,
                "workers_min": min(int(row["workers_used"]) for row in rows),
                "workers_max": max(int(row["workers_used"]) for row in rows),
            }
        summary[scene] = {
            "frames": len(reference), "warmup": warm,
            "n": int(reference[0]["n"]), "seed": int(reference[0]["seed"]),
            "pair_occurrences": sum(int(row["exact_pairs"]) for row in reference),
            "methods": times,
        }
    return {
        "scope": "synthetic AABB count+commutative fingerprint, NOT emitted pair lists",
        "reference": "persistent flat grid 1-thread, same scene and seed",
        "source_files": manifests, "scenes": summary,
        "warnings": [
            "This cohort is not comparable to full pair-output material replay",
            "Reported complete cost includes preflight and index maintenance, excludes physics",
            "Warm speedups exclude cold initialization; no cross-machine speedup inference",
        ],
    }


def report(data: dict, destination: Path) -> None:
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite results: {destination}")
    destination.mkdir(parents=True)
    (destination / "KYOTO_UNIFIED_EVIDENCE.json").write_text(
        json.dumps(data, indent=2) + "\n", encoding="utf-8")
    mat = data["material_replay"]
    lines = [
        "# Kyoto: unified evidence, NOT unified performance claims", "",
        "## A. The original Online / Global Guard / SAP / ML comparison", "",
        f"**{mat['trajectories']} complete trajectories** with the *same* candidate-list "
        "semantics within each replay. The reference is Global Guard.", "",
        "| Method | Scene-level geomean Guard/method | Wins vs Guard | Sum-of-means speedup |",
        "|---|---:|---:|---:|",
    ]
    for name, metrics in sorted(mat["methods"].items(),
                                key=lambda item: -item[1]["guard_speedup_geomean"]):
        lines.append(
            f"| {name} | {metrics['guard_speedup_geomean']:.4f}× | "
            f"{metrics['trajectory_wins_vs_guard']}/{mat['trajectories']} | "
            f"{metrics['summed_scene_mean_speedup_vs_guard']:.4f}× |")
    lines += ["", "A missing ML or Bullet row means **not executed**, not a loss or zero time.",
              "Full candidate pair lists are generated and checked, but no physical solver "
              "or continuous collision detection is measured.", ""]
    phase16 = data["phase16"]
    if phase16:
        lines += ["## B. Phase 16 persistent spatial-index pressure (separate cohort)", "",
                  "This records **count+fingerprints**, not actual emitted candidate lists. "
                  "Do not directly rank these numbers against section A.", "",
                  "| Synthetic scene | Threads | Warm full ms | Warm speedup vs 1 thread | Actual workers |",
                  "|---|---:|---:|---:|---:|"]
        for scene, info in phase16["scenes"].items():
            for threads, values in info["methods"].items():
                lines.append(f"| {scene} | {threads} | {values['warm_full_ms']:.4f} | "
                             f"{values['speedup_vs_same_scene_1thread']:.3f}× | "
                             f"{values['workers_min']}–{values['workers_max']} |")
    else:
        lines += ["## B. Phase 16", "", "Not supplied; no synthetic timings invented."]
    lines += ["", "## Research boundaries", "",
              "This report never calls an offline per-frame minimum an online controller; "
              "never equates broad-phase candidates with physical contacts; and never "
              "treats two different input cohorts as the same benchmark.",
              "", "Source SHA-256 values and the machine-readable frame-level scoring "
              "are preserved in KYOTO_UNIFIED_EVIDENCE.json.", ""]
    (destination / "KYOTO_UNIFIED_EVIDENCE.md").write_text(
        "\n".join(lines), encoding="utf-8")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--materials", type=Path, required=True,
                   help="Folder containing replays/*-evaluated.csv")
    p.add_argument("--phase16", type=Path,
                   help="Optional directory of *-threads1/2/4/8.csv files")
    p.add_argument("--warmup", type=int, default=4)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = {
        "material_replay": material_results(args.materials, args.warmup),
        "phase16": synthetic_results(args.phase16) if args.phase16 else None,
    }
    report(result, args.output)
    print(f"PASS: {len(result['material_replay']['methods'])} material algorithms "
          f"and {len(result['phase16']['scenes']) if result['phase16'] else 0} "
          f"separate pressure scenes; {args.output}")


if __name__ == "__main__":
    main()
