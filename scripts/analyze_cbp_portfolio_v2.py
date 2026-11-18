#!/usr/bin/env python3
"""Audit exact source hashes, executed on-policy arm choices and honest costs.

The frozen six-method CPU tournament is a SEPARATE process. Comparing
mean times across files is descriptive, NOT a per-frame concurrent oracle or
a statistically controlled causal improvement. The trial manager never reads
the reference CSV and never executes an unchosen backend in its timed query.
"""
from __future__ import annotations
import argparse
import csv
import math
from pathlib import Path
import statistics


def records(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as stream:
        r = list(csv.DictReader(stream))
    if not r:
        raise ValueError("Empty benchmark CSV: " + str(path))
    return r


def stats(values: list[float]) -> tuple[float, float]:
    if not values or not all(math.isfinite(x) and x >= 0 for x in values):
        raise ValueError("Invalid positive cost distribution")
    ordered = sorted(values)
    return statistics.fmean(ordered), ordered[math.ceil(.95 * len(ordered)) - 1]


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("four_arm_csv", type=Path)
    p.add_argument("frozen_cpu_csv", type=Path)
    p.add_argument("--report", type=Path, required=True)
    args = p.parse_args()
    if args.report.exists():
        raise FileExistsError("Cannot overwrite existing experimental report")
    manager = records(args.four_arm_csv)
    original = records(args.frozen_cpu_csv)
    reference = {
        int(x["frame"]): x for x in original if x["mode"] == "sap_rebuild"
    }
    frozen = {
        key: sorted(
            (x for x in original if x["mode"] == key),
            key=lambda x: int(x["frame"])
        )
        for key in set(x["mode"] for x in original)
    }
    if len(reference) != len(manager) or len(manager) < 12:
        raise ValueError("Manager and frozen methods evaluated different frame counts")
    expected = set(range(len(manager)))
    if set(reference) != expected or any(
        int(x["frame"]) != i for i, x in enumerate(manager)
    ):
        raise ValueError("Discontinuous or duplicated frame IDs")
    if any(
        len(values) != len(manager) or
        any(int(row["frame"]) != i for i, row in enumerate(values))
        for values in frozen.values()
    ):
        raise ValueError("Frozen control missing frame/algorithm")
    executed = {}
    transitions = []
    for i, row in enumerate(manager):
        oracle = reference[i]
        if (
            row["exact_parity"] != "verified" or
            row["pair_hash"] != row["oracle_pair_hash"] or
            row["pair_hash"] != oracle["pair_hash"] or
            int(row["pairs"]) != int(oracle["pairs"]) or
            int(row["inter_instance_pairs"]) != int(oracle["inter_instance_pairs"])
        ):
            raise ValueError(f"Full exact source parity mismatch at frame {i}")
        arm = row["mode"]
        if arm not in {"frozen_v4", "shared_bvh", "region_sweep", "region_exhaustive"}:
            raise ValueError("Unrecognized four-arm mode")
        executed[arm] = executed.get(arm, 0) + 1
        if row["switched"] == "1":
            transitions.append(
                (i, arm, row["champion"], row["probe"], row["returning"])
            )
        q = float(row["query_ms"])
        reported = float(row["internal_total_ms"])
        backend = float(row["backend_ms"])
        migration = float(row["migration_ms"])
        if (
            not math.isfinite(q) or not math.isfinite(reported) or
            not math.isfinite(backend) or not math.isfinite(migration) or
            q <= 0 or reported < backend or q < .7 * reported
        ):
            raise ValueError(f"Unaccounted/invalid probe timing at frame {i}")
    warmup = min(8, len(manager) // 8)
    v2_mean, v2_p95 = stats([float(x["query_ms"]) for x in manager[warmup:]])
    frozen_mean, _ = stats(
        [float(x["ms"]) for x in frozen["frozen_v4"][warmup:]]
    )
    individual = {
        mode: stats([float(x["ms"]) for x in vals[warmup:]])
        for mode, vals in frozen.items()
        if mode != "sap_rebuild"
    }
    fast = min(individual, key=lambda k: individual[k][0])
    fastest = individual[fast][0]
    lines = [
        "# Four-arm on-policy portfolio — independent-execution evaluation",
        "",
        f"**Frames:** {len(manager)}, {len(manager) - warmup} timed after warmup.",
        "",
        "All manager candidate pairs were checked against the FULL frozen SAP "
        "reference set (sorted stable-ID pair hashes and counts), frame by frame. "
        "The actual manager route had NO access to those offline reference timings.",
        "",
        "| Algorithm | Mean complete CPU query (ms) | P95 (ms) | V4 / method |",
        "|---|---:|---:|---:|",
        f"| **New four-arm causal portfolio** | **{v2_mean:.5f}** | "
        f"**{v2_p95:.5f}** | **{frozen_mean/v2_mean:.3f}×** |",
        f"| Frozen V4 reference | {frozen_mean:.5f} | "
        f"{stats([float(x['ms']) for x in frozen['frozen_v4'][warmup:]])[1]:.5f}"
        " | 1.000× |",
    ]
    for arm, (mean, p95) in sorted(individual.items(), key=lambda kv: kv[1][0]):
        if arm == "frozen_v4":
            continue
        lines.append(f"| {arm} (offline fixed control) | {mean:.5f} | "
                     f"{p95:.5f} | {frozen_mean/mean:.3f}× |")
    lines += [
        "",
        f"**Offline best fixed comparator:** {fast} ({fastest:.5f} ms); "
        f"on-policy manager / best-fixed cost ratio = {v2_mean/fastest:.3f}×. "
        "This ratio is descriptive and NOT a deployment oracle.",
        "",
        "## Actually executed arms",
        "",
    ]
    for arm, count in sorted(executed.items()):
        lines.append(f"- {arm}: {count} real frames")
    final = manager[-1]
    lines += [
        "",
        f"Full-cohort recorded trials={final['trial_count']}, "
        f"promotions={final['promotions']}, rejections={final['rejections']}, "
        f"switches={final['switch_count']}.",
        "",
        "## Mode changes (charged in that frame's observed cost)",
        "",
        "| Zero-based frame | Selected exact backend | Current champion | Probe | Returning |",
        "|---:|---|---|---|---|",
    ]
    for t in transitions:
        lines.append(f"| {t[0]} | {t[1]} | {t[2]} | {t[3]} | {t[4]} |")
    lines += [
        "",
        "## Research caveats",
        "",
        "1. Phase 10 is scene-level whole-backend delegation, not heterogeneous "
        "per-region mixed execution.",
        "2. The frozen fixed-policy controls were measured in an independent "
        "process/run. Different CPU scheduling, caches, noise, and chronology "
        "can affect the apparent gain. A/B repetition with alternated order "
        "and held-out seeds is required before a performance claim.",
        "3. Cold switches are paid, and the manager performs no shadow "
        "backend queries or offline future-winner lookups.",
        "4. The real GLB inputs are deterministic, rigidly animated, sampled "
        "source triangles; broad-phase overlaps are not resolved mesh contacts.",
        "5. Exact pair equality, though strong evidence, is only discrete "
        "current-AABB completeness—not CCD or full deformable solver quality.",
    ]
    report = "\n".join(lines) + "\n"
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(report, encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
