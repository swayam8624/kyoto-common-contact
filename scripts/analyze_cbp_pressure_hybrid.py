#!/usr/bin/env python3
"""Phase 13 experiment audit. Compare actual emitted pair digests and honest
whole-query costs against separately executed frozen global CPU baselines.

Never rank a truncated global X sweep as if its elapsed budget-stop time were
the exact completed method's latency. The hybrid uses no offline winner info.
"""
from __future__ import annotations
import argparse
import csv
from pathlib import Path
import math
import statistics


FIELDS = ("exact_pairs", "hash64_sum", "hash64_xor",
          "hash32_sum", "hash32_xor")


def load(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def summary(values: list[float]):
    if not values:
        return "—", "—"
    if not all(math.isfinite(x) and x >= 0 for x in values):
        raise ValueError("Nonfinite timing")
    return (f"{statistics.fmean(values):.5f}",
            f"{sorted(values)[math.ceil(.95*len(values))-1]:.5f}")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("hybrid", type=Path)
    p.add_argument("fixed", type=Path)
    p.add_argument("--report", type=Path, required=True)
    a = p.parse_args()
    if a.report.exists():
        raise FileExistsError("Do not overwrite a prior research report")
    rows = load(a.hybrid)
    frozen = load(a.fixed)
    if not rows or len(frozen) != len(rows)*2:
        raise ValueError("Frame count differs between controller and frozen references")
    if any(int(r["frame"]) != i for i,r in enumerate(rows)):
        raise ValueError("Manager frame order corrupted")
    means = {"hybrid_total":[], "hybrid_query_only":[],
             "fixed_grid_total":[], "fixed_sweep_query":[],
             "policy_decision":[], "policy_preflight":[]}
    choices = {"global_3d_hash_grid":0, "projected_x_sweep":0}
    grid_complete, sweep_complete = 0,0
    for i,record in enumerate(rows):
        if record["status"] != "complete":
            raise ValueError(f"Hybrid frame {i} did not finish exact enumeration")
        group = frozen[2*i:2*i+2]
        if [r["method"] for r in group] != [
            "global_3d_hash_grid", "global_x_sweep"
        ] or any(int(r["frame"])!=i for r in group):
            raise ValueError("Fixed reference rows misaligned")
        for ref in group:
            if ref["status"] != "complete":
                continue
            if any(record[key] != ref[key] for key in FIELDS):
                raise ValueError(f"Pair digest mismatch {ref['method']} frame {i}")
        choices[record["selected_backend"]] += 1
        means["hybrid_total"].append(float(record["complete_query_ms"]))
        means["hybrid_query_only"].append(float(record["query_ms"]))
        means["policy_decision"].append(float(record["decision_ms"]))
        means["policy_preflight"].append(float(record["estimator_ms"]))
        g,s=group
        if g["status"]=="complete":
            grid_complete+=1
            means["fixed_grid_total"].append(
                float(g["grid_build_ms"])+float(g["query_ms"])
            )
        if s["status"]=="complete":
            sweep_complete+=1
            means["fixed_sweep_query"].append(float(s["query_ms"]))
    title = rows[0]["scene"]
    n = int(rows[0]["n"])
    lines = [
        f"# Geometric global CPU router: {title}, N={n:,}",
        "",
        f"Exact completed manager frames: {len(rows)}/{len(rows)}.",
        f"Frozen global-grid frames complete: {grid_complete}/{len(rows)}.",
        f"Frozen X-sweep frames complete: {sweep_complete}/{len(rows)}.",
        "",
        "| Measurement | Mean ms | P95 ms | Included cost |",
        "|---|---:|---:|---|",
    ]
    desc={
        "hybrid_total":"Manager + grid build + geometric estimation + selected query",
        "hybrid_query_only":"Only selected backend's pair enumeration",
        "fixed_grid_total":"Fixed baseline rebuild + complete 3D-grid query",
        "fixed_sweep_query":"Fixed sweep sort + full query; no grid built",
        "policy_decision":"Geometric decision incl conditional sorted-X preflight",
        "policy_preflight":"Subset of decision: sorting and projected-pair calculation",
    }
    for key, values in means.items():
        m,p95 = summary(values)
        lines.append(f"| {key} | {m} | {p95} | {desc[key]} |")
    lines.extend([
        "",
        "## Actual on-policy modes",
        "",
        f"- 3D grid actually executed on {choices['global_3d_hash_grid']} frames",
        f"- X sweep actually executed on {choices['projected_x_sweep']} frames",
        "",
        "## Interpretation",
        "",
        "The policy never executed the unchosen backend while measuring its "
        "own query, and it consulted only current Float32 AABB geometry "
        "and the grid's exact cell-pair work upper bound.",
        "",
        "All completed frame fingerprints were checked against every "
        "available separately executed *completed* fixed reference. "
        "Incomplete fixed sweep rows are not valid correctness or timing "
        "controls. For small scenes, a third brute-force oracle also runs.",
        "",
        "These times come from different processes and may have noise. "
        "Grid construction is always paid by this prototype, including when "
        "the X sweep wins, so it does not imply optimal end-to-end routing.",
        "The metric here is count+fingerprint, NOT the time to emit every pair "
        "to a GPU/CPU buffer or to solve physical contact constraints.",
        "The 128*N preflight threshold and /2 work ratio are explicit "
        "heuristics, NOT a proof of a universal optimum or an ML-trained policy.",
        "There is no novel graphics research claim until a proper interaction "
        "representation, independent workload tests, and stronger external "
        "baselines have been demonstrated.",
        "",
    ])
    output="\n".join(lines)
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(output,encoding="utf-8")
    print(output)
    return 0


if __name__=="__main__":
    raise SystemExit(main())
