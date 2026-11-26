#!/usr/bin/env python3
"""Pass 37: paired-block summaries; no external dependencies.

A trajectory block, not a leaf or individual query, is the timing unit.
Five blocks on one machine are repeated measurements, not independent
physical populations. Bootstrap intervals describe repeatability only.
"""
import argparse
import csv
import json
import math
import random
import statistics
from collections import defaultdict
from pathlib import Path


def summarize(source, output):
    cells = defaultdict(dict)
    with source.open(newline="") as handle:
        for row in csv.DictReader(handle):
            key = (int(row["width_cells"]), row["regime"], int(row["block"]))
            arm = row["arm"]
            leaf = int(row["leaf"])
            elapsed = float(row["query_ns"])
            if not math.isfinite(elapsed) or elapsed <= 0:
                raise ValueError(f"invalid measured time: {key}, {arm}, {leaf}")
            slots = cells[key].setdefault(arm, {})
            if leaf in slots:
                raise ValueError(f"duplicate leaf: {key}, {arm}, {leaf}")
            slots[leaf] = (elapsed, int(row["candidates"]), int(row["vertices"]),
                           int(row["edges"]), int(row["faces"]))
    if not cells:
        raise ValueError("empty benchmark receipt")
    arms = {"exhaustive", "rebuilt_sap", "rebuilt_bvh", "retained_refit",
            "retained_selective_frontier"}
    ratios = defaultdict(list)
    for (width, regime, block), measured in sorted(cells.items()):
        if set(measured) != arms:
            raise ValueError(f"incomplete arm set at {(width, regime, block)}")
        base = measured["rebuilt_sap"]
        expected = {i: x[1:] for i, x in base.items()}
        for arm, records in measured.items():
            if {i: x[1:] for i, x in records.items()} != expected:
                raise ValueError("unmatched physical source/count receipt")
            total = math.fsum(x[0] for x in records.values())
            reference = math.fsum(x[0] for x in base.values())
            ratios[(width, regime, arm)].append((block, reference / total, total))
    rng = random.Random(3702026)
    rows = []
    for (width, regime, arm), records in sorted(ratios.items()):
        if {r[0] for r in records} != set(range(5)):
            raise ValueError("missing measured blocks")
        logs = [math.log(r[1]) for r in records]
        boot = sorted(math.exp(statistics.mean(rng.choices(logs, k=len(logs))))
                      for _ in range(10000))
        rows.append(dict(width_cells=width, regime=regime, arm=arm,
                         blocks=len(records),
                         geomean_sap_over_arm=math.exp(statistics.mean(logs)),
                         min_sap_over_arm=min(r[1] for r in records),
                         max_sap_over_arm=max(r[1] for r in records),
                         repeatability_bootstrap_low=boot[250],
                         repeatability_bootstrap_high=boot[9749],
                         median_trajectory_query_ns=statistics.median(r[2] for r in records)))
    output.write_text(json.dumps({
        "comparison": "rebuilt SAP total trajectory query time / arm total trajectory query time",
        "scope": "candidate-only; shared snapshot assembly and physical integration excluded",
        "interval_warning": "within-machine repeatability only; five blocks do not establish population generality",
        "rows": rows}, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    summarize(args.input, args.output)
