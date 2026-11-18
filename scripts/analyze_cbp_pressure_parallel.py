#!/usr/bin/env python3
"""Compare exact serial/parallel flat-cell queries on identical frame geometry.

Both paths preserve complete per-frame digests; fixed controls run in a
separate process. No simultaneous 'best backend' is used by the selection.
We never infer hardware utilization from wall-clock speedup.
"""
from __future__ import annotations
import argparse
import csv
import math
from pathlib import Path
import statistics

FIELDS=("exact_pairs","hash64_sum","hash64_xor","hash32_sum","hash32_xor")
TIME=("complete_query_ms","query_ms","preflight_ms","index_maintenance_ms",
      "decision_ms")


def read(p: Path) -> list[dict]:
    with p.open(newline="",encoding="utf-8") as f:
        return list(csv.DictReader(f))

def stats(rs: list[dict],key: str)->tuple[float,float]:
    values=sorted(float(r[key]) for r in rs)
    if not values or not all(math.isfinite(x) and x>=0 for x in values):
        raise ValueError("Invalid timing distribution "+key)
    return statistics.fmean(values),values[math.ceil(len(values)*.95)-1]


def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("serial",type=Path)
    p.add_argument("parallel",type=Path)
    p.add_argument("frozen",type=Path)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():
        raise FileExistsError("Refuse to overwrite previous evidence")
    serial,parallel,frozen=read(a.serial),read(a.parallel),read(a.frozen)
    if not serial or len(serial)!=len(parallel) or len(frozen)!=2*len(serial):
        raise ValueError("Incomplete comparator cohort")
    scene=serial[0]["scene"]
    n=int(serial[0]["n"])
    workers=[]
    full_controls=[]
    false_tests=0
    for i,(s,r) in enumerate(zip(serial,parallel)):
        assert int(s["frame"])==int(r["frame"])==i
        assert s["status"]==r["status"]=="complete"
        assert s["selected_pair_tests"]==r["selected_pair_tests"]
        assert s["grid_work_upper_bound"]==r["grid_work_upper_bound"]
        assert s["thread_limit"]=="1"
        assert int(r["thread_limit"])>=2
        workers.append(int(r["workers_used"]))
        false_tests+=int(r["selected_pair_tests"])-int(r["exact_pairs"])
        for key in FIELDS:
            if s[key]!=r[key]:
                raise ValueError(f"Serial/parallel full count+digest mismatch frame {i}, {key}")
        controls=frozen[2*i:2*i+2]
        if controls[0]["method"]!="global_3d_hash_grid" or (
            controls[1]["method"]!="global_x_sweep"
        ):
            raise ValueError("Frozen grid/sweep controls out of order")
        for c in controls:
            if c["status"]=="complete":
                if any(c[k]!=r[k] for k in FIELDS):
                    raise ValueError(f"Independent complete fixed oracle mismatch frame {i}")
        if controls[0]["status"]=="complete":
            full_controls.append(
                float(controls[0]["grid_build_ms"])+float(controls[0]["query_ms"])
            )
    warmup=1 if len(serial)>=5 else 0
    s=serial[warmup:]
    r=parallel[warmup:]
    serial_mean,serial_p95=stats(s,"complete_query_ms")
    parallel_mean,parallel_p95=stats(r,"complete_query_ms")
    serial_query,_=stats(s,"query_ms")
    parallel_query,_=stats(r,"query_ms")
    frozen_mean=statistics.fmean(full_controls) if full_controls else None
    lines=[
      f"# Phase 16: exact bounded CPU parallel canonical-cell query ({scene}, N={n:,})",
      "",
      f"- Completed exact frames: **{len(serial)}/{len(serial)}**, "
      f"warmup excluded: {warmup}",
      f"- Actual parallel workers used: min **{min(workers)}**, max **{max(workers)}**",
      f"- Total visited cell pairs: **{sum(int(x['selected_pair_tests']) for x in parallel):,}**",
      f"- Total true exact candidates: **{sum(int(x['exact_pairs']) for x in parallel):,}**",
      f"- Pair visits without accepted exact candidates: **{false_tests:,}** "
      "(includes canonical-cell duplicate suppression)",
      "",
      "| Measurement | Mean complete ms | P95 complete ms | Mean enumeration ms |",
      "|---|---:|---:|---:|",
      f"| Serial flat grid (1 thread) | {serial_mean:.4f} | {serial_p95:.4f} | {serial_query:.4f} |",
      f"| Parallel flat grid (requested {parallel[0]['thread_limit']} threads) | "
      f"{parallel_mean:.4f} | {parallel_p95:.4f} | {parallel_query:.4f} |",
      f"| Fixed rebuilt grid (independent process; all frames) | "
      f"{frozen_mean:.4f} | — | — |" if frozen_mean is not None else
      "| Fixed rebuilt grid | incomplete | — | — |",
      "",
      f"**Serial / parallel whole-query ratio after warmup:** "
      f"**{serial_mean/parallel_mean:.3f}×**.",
      f"**Serial / parallel enumeration ratio after warmup:** "
      f"**{serial_query/parallel_query:.3f}×**.",
      "",
      "Parallel mode executes only one grid query, with independent read-only "
      "cell chunks, local full count+commutative digests, then deterministic "
      "reduction. It pays per-frame thread scheduling, chunking and joining.",
      "Safety caps and the separate X-sweep fallback remain in effect. "
      "An incomplete frame cannot count as an exact win.",
      "Phase 16 does not eliminate the Ω(K) work required to explicitly "
      "materialize K collision candidate pairs. It currently measures "
      "count+fingerprints, NOT materialized complete pair output.",
      "This is ordinary multicore CPU scheduling, not an original graphics "
      "mathematical theorem. Results are exploratory and vulnerable to OS "
      "scheduling/caching/thermal and benchmark order.",
      "",
    ]
    report="\n".join(lines)
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(report,encoding="utf-8")
    print(report)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
