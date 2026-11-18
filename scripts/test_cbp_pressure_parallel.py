#!/usr/bin/env python3
"""Phase 16 full count/hash oracle for 1/4 worker flat-grid comparisons.

This is a correctness test, NOT a claim that 4 workers are faster.
Every method is timed in its own process, uses the same AABB source,
and must match the independent rebuilt-grid reference's COMPLETE digest.
The all-overlap case exercises the unchanged safe single X-sweep fallback.
"""
from __future__ import annotations
import csv
from pathlib import Path
import subprocess
import sys
import tempfile

HASHES=("exact_pairs","hash64_sum","hash64_xor","hash32_sum","hash32_xor")

def execute(args: list[str]):
    p=subprocess.run(args,capture_output=True,text=True,timeout=90)
    if p.returncode:
        raise AssertionError(
            f"Native command failed: {args}\n{p.stdout}\n{p.stderr}")

def read(filename: Path) -> list[dict]:
    with filename.open(newline="",encoding="utf-8") as f:
        return list(csv.DictReader(f))

def verify(flat: str,fixed: str,folder: Path,scene: str,n: int,frames: int,
           seed: int):
    source=["%s"%scene,str(n),str(frames),str(seed),"80000000","15000","3000000"]
    serial=folder/f"{scene}-{n}-serial.csv"
    parallel=folder/f"{scene}-{n}-parallel.csv"
    independent=folder/f"{scene}-{n}-frozen.csv"
    execute([flat,str(serial),*source,"--threads","1"])
    execute([flat,str(parallel),*source,"--threads","4"])
    execute([fixed,str(independent),*source])
    a,b,controls=read(serial),read(parallel),read(independent)
    assert len(a)==len(b)==frames and len(controls)==2*frames
    allocated_parallel=False
    for f,(normal,multi) in enumerate(zip(a,b)):
        assert int(normal["frame"])==int(multi["frame"])==f
        assert normal["status"]==multi["status"]=="complete"
        assert normal["thread_limit"]=="1" and multi["thread_limit"]=="4"
        assert normal["workers_used"]=="1"
        assert float(normal["complete_query_ms"])>=float(normal["query_ms"])
        assert float(multi["complete_query_ms"])>=float(multi["query_ms"])
        assert normal["grid_work_upper_bound"]==multi["grid_work_upper_bound"]
        assert normal["selected_pair_tests"]==multi["selected_pair_tests"]
        for key in HASHES:
            assert normal[key]==multi[key],(scene,n,f,key)
        allocated_parallel |= int(multi["workers_used"])>=2
        for control in controls[2*f:2*f+2]:
            if control["status"]!="complete":
                continue
            for key in HASHES:
                assert control[key]==multi[key],(scene,n,f,key)
        if scene=="all_overlap":
            assert multi["selected_backend"]=="projected_x_sweep"
            assert int(multi["exact_pairs"])==n*(n-1)//2
            assert multi["workers_used"]=="1"
    if scene!="all_overlap":
        assert allocated_parallel,(
            "Parallel backend not actually executed",scene,n)
    print(f"PASS Phase16 exact {scene} N={n} frames={frames}, "
          f"genuine_parallel={allocated_parallel}")

def main():
    if len(sys.argv)!=3:
        raise SystemExit("Usage: test_cbp_pressure_parallel.py FLAT_EXEC FROZEN_EXEC")
    flat,fixed=[str(Path(x).resolve()) for x in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix="cbp-parallel-exact-") as tmp:
        root=Path(tmp)
        verify(flat,fixed,root,"slabs",65536,6,92113)
        verify(flat,fixed,root,"burst",16384,10,92113)
        verify(flat,fixed,root,"all_overlap",1024,4,92113)
    print("PASS Phase16 exact serial/parallel/frozen canonical-cell digests")

if __name__=="__main__":
    main()
