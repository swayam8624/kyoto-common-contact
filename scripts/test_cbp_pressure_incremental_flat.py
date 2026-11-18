#!/usr/bin/env python3
"""Exact/cost audit for persistent canonical-cell broad phase.

Original frozen CPU grid and X sweep run in a DIFFERENT process, using the
same deterministic generated AABBs. Neither is executed inside the incremental
timed path; only small-scene brute force runs after it to validate output.
"""
from __future__ import annotations
import csv
import math
from pathlib import Path
import subprocess
import sys
import tempfile

FIELDS=("exact_pairs","hash64_sum","hash64_xor","hash32_sum","hash32_xor")


def run(argv: list[str], timeout=90):
    p=subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    if p.returncode:
        raise AssertionError(f"Exit {p.returncode}: {' '.join(argv)}\n"
                             f"STDOUT:\n{p.stdout}\nSTDERR:\n{p.stderr}")


def read(file: Path) -> list[dict]:
    with file.open(newline="",encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def check(cpu: str, persistent: str, folder: Path, name: str,
          n: int, frames: int, max_tests: int):
    a=folder/f"{name}-{n}-persistent.csv"
    b=folder/f"{name}-{n}-frozen.csv"
    args=[name,str(n),str(frames),"72311",
          str(max_tests),"15000","2000000"]
    run([persistent,str(a),*args],timeout=90)
    run([cpu,str(b),*args],timeout=90)
    observed=read(a)
    baseline=read(b)
    assert len(observed)==frames and len(baseline)==2*frames
    any_dirty=False
    for k,row in enumerate(observed):
        assert int(row["frame"])==k
        assert row["status"]=="complete", (name,k,row["status"])
        assert row["selected_backend"] in {
            "persistent_flat_3d_grid","projected_x_sweep"
        }
        assert float(row["complete_query_ms"])>=float(row["query_ms"])
        assert int(row["grid_entries"])>0
        assert int(row["active_cells"])>0
        assert int(row["peak_rss_mib"].split(".")[0])>=0
        assert k==0 or int(row["changed_cell_ranges"])<=n
        assert row["analytical_construct_verified"] in ("0","1")
        if n<=1024:
            assert row["brute_oracle_checked"]=="1"
        any_dirty |= int(row["changed_cell_ranges"])>0
        for fixed in baseline[2*k:2*k+2]:
            if fixed["status"]!="complete":
                continue
            for field in FIELDS:
                assert row[field]==fixed[field], (
                    name,n,k,field,row[field],fixed[field]
                )
        if name=="slabs":
            assert row["analytical_construct_verified"]=="1"
            assert row["selected_backend"]=="persistent_flat_3d_grid"
            assert row["exact_pairs"]=="0"
            if k>=1:
                assert int(row["changed_cell_ranges"])==0, \
                    "No moving cell ranges in slab scene"
                assert row["rebuilt"]=="0"
        if name=="all_overlap":
            assert row["selected_backend"]=="projected_x_sweep"
            assert int(row["exact_pairs"])==n*(n-1)//2
            assert row["analytical_construct_verified"]=="1"
            if k>=1:
                assert row["rebuilt"]=="0"
                assert int(row["changed_cell_ranges"])==0
        if k==0:
            assert row["rebuilt"]=="1"
    assert any_dirty
    print("PASS FLAT FLAT persistent exact AABB truth:",name,"N=",n,
          "frames=",frames,
          "rebuilt=",sum(int(x["rebuilt"]) for x in observed),
          "dirty=",sum(int(x["changed_cell_ranges"]) for x in observed),
          "executed=",
          sorted(set(x["selected_backend"] for x in observed)))


def main():
    if len(sys.argv)!=3:
        raise SystemExit("Usage: test_cbp_pressure_incremental.py FROZEN_CPU PERSISTENT_CPU")
    fixed,inc=(str(Path(x).resolve()) for x in sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="cbp-incremental-exact-") as tmp:
        base=Path(tmp)
        for scene in ("sparse","burst","slabs","bridge",
                      "teleport","all_overlap"):
            check(fixed,inc,base,scene,192,12,3_000_000)
        check(fixed,inc,base,"all_overlap",8192,2,50_000_000)
        check(fixed,inc,base,"slabs",65536,5,8_000_000)
    print("PASS eight independent cohort exactness checks, quiet frame reuse, "
          "canonical cell duplicate suppression and dense X sweep")


if __name__=="__main__":
    main()
