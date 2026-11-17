#!/usr/bin/env python3
"""Describe V4-versus-CBP replay evidence without silently tuning the manager.

Reads an already generated per-frame trace. Never reruns simulations, never
claims independent significance, and never overwrites evidence files.
"""
from __future__ import annotations

import collections
import csv
import math
import pathlib
import statistics
import sys


def number(row: dict[str, str], key: str) -> float:
    value = float(row[key])
    if not math.isfinite(value):
        raise ValueError(f"Nonfinite value for {key}")
    return value


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 scripts/analyze_cbp_pilot.py TRACE.csv")
    path = pathlib.Path(sys.argv[1])
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        required = {
            "frame", "method", "query_ms", "exact_pairs", "manager_mode",
            "v4_action", "v4_trial", "v4_switched", "manager_sample_density",
            "manager_sample_motion", "manager_sample_count", "local_primitive_tests",
            "cross_primitive_tests", "cache_graph_builds", "cache_quiet_reuses",
            "cache_dirty", "cache_potential_pairs", "regions",
        }
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise ValueError("Trace predates the instrumented replay or is incomplete")
        rows = list(reader)
    if not rows:
        raise ValueError("Empty CBP pilot trace")
    by_method: dict[str, list[dict[str, str]]] = collections.defaultdict(list)
    by_frame: dict[int, list[dict[str, str]]] = collections.defaultdict(list)
    for row in rows:
        by_method[row["method"]].append(row)
        by_frame[int(row["frame"])].append(row)
    expected = {
        "sap_rebuild", "v4_guard_sap_bounded_trial", "region_exhaustive",
        "region_shared_bvh", "region_certified_cache", "causal_cbp",
    }
    # Older six-policy traces remain valid immutable evidence.
    # New instrumented replays add a separately timed V4+regional portfolio.
    if "portfolio_v4_regional" in by_method:
        expected.add("portfolio_v4_regional")
    if set(by_method) != expected:
        raise ValueError("Missing or unrecognized replay policy")
    for frame, entries in by_frame.items():
        if len(entries) != len(expected) or {r["method"] for r in entries} != expected:
            raise ValueError(f"Incomplete or duplicate frame {frame}")
        if len({r["exact_pairs"] for r in entries}) != 1:
            raise ValueError(f"Pair-count mismatch at frame {frame}")
    n = len(by_frame)
    v4 = by_method["v4_guard_sap_bounded_trial"]
    manager = by_method["causal_cbp"]
    base_sum = sum(number(r, "query_ms") for r in v4)
    print(f"# CBP pilot for {path.name} — {n} timed frames")
    print()
    print("This is a descriptive analysis of one inspected replay, not a significance test.")
    print()
    print("| Method | Mean ms | Ratio V4 / method | Mean local tests | Mean cross tests |")
    print("|---|---:|---:|---:|---:|")
    for name, items in by_method.items():
        elapsed = sum(number(r, "query_ms") for r in items)
        local = sum(number(r, "local_primitive_tests") for r in items) / n
        cross = sum(number(r, "cross_primitive_tests") for r in items) / n
        print(f"| {name} | {elapsed/n:.6f} | {base_sum/elapsed:.3f}x | {local:.1f} | {cross:.1f} |")
    actions = collections.Counter(row["v4_action"] for row in v4)
    modes = collections.Counter(row["manager_mode"] for row in manager)
    trials = sum(int(row["v4_trial"]) for row in v4)
    switches = sum(int(row["v4_switched"]) for row in v4)
    print()
    print("## Actual backend choice (not inferred from names)")
    print()
    print(f"V4 actions: {dict(actions)}; SAP trial frames={trials}; backend switches={switches}.")
    print(f"CBP choices: {dict(modes)}; transitions={sum(int(r['switched']) for r in manager)}.")
    if "portfolio_v4_regional" in by_method:
        portfolio = by_method["portfolio_v4_regional"]
        choices = collections.Counter(r["portfolio_mode"] for r in portfolio)
        trials = sum(int(r["portfolio_trial"]) for r in portfolio)
        accepted = sum(int(r["portfolio_accepted"]) for r in portfolio)
        rejected = sum(int(r["portfolio_rejected"]) for r in portfolio)
        print(f"V4+regional portfolio choices: {dict(choices)}; trial frames={trials}; "
              f"accepted trials={accepted}; rejected trials={rejected}.")
        print("Portfolio trials are executed, timed, and charged on-policy; "
              "no alternative algorithm runs inside the manager.")
    densities = [number(r, "manager_sample_density") for r in manager]
    motions = [number(r, "manager_sample_motion") for r in manager]
    print(f"CBP sampled cross-region overlap density: min={min(densities):.3f}, "
          f"median={statistics.median(densities):.3f}, max={max(densities):.3f}.")
    print(f"CBP sampled moved-box fraction: min={min(motions):.3f}, "
          f"median={statistics.median(motions):.3f}, max={max(motions):.3f}.")
    print()
    print("## Certified-graph work (including negative evidence)")
    print()
    cert = by_method["region_certified_cache"]
    for key in ("cache_graph_builds", "cache_quiet_reuses", "cache_dirty", "cache_potential_pairs"):
        values = [number(r, key) for r in cert]
        print(f"{key}: total={sum(values):.0f}; median/frame={statistics.median(values):.1f}; max/frame={max(values):.0f}")
    print()
    print("Note: counts are instrumentation of algorithmic work, not isolated CPU-stage times.")
    print("Exact candidate-set parity must be established by the replay executable.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
