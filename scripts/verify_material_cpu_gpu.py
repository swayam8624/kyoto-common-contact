#!/usr/bin/env python3
"""Fail closed if Metal and CPU replay disagree on ANY material collision pair."""
import argparse
import csv
from pathlib import Path
from statistics import median

def records(path):
    with Path(path).open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cpu_csv")
    parser.add_argument("metal_csv")
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    cpu=records(args.cpu_csv)
    gpu=records(args.metal_csv)
    reference={int(row["frame"]):row for row in cpu if row["backend"]=="sap_rebuild"}
    online={int(row["frame"]):row for row in cpu if row["backend"]=="online_cost_aware"}
    if not reference or len(online)!=len(reference):
        raise RuntimeError("missing or incomplete CPU reference / online controller")
    if len(gpu)!=len(reference):
        raise RuntimeError("GPU and CPU have different frame counts")
    gpu_frames=set()
    for row in gpu:
        frame=int(row["frame"])
        if frame in gpu_frames or frame not in reference:
            raise RuntimeError("unexpected or repeated GPU frame")
        gpu_frames.add(frame)
        ref=reference[frame]
        if int(ref["n"])!=int(row["n"]) or ref["seed"]!=row["seed"]:
            raise RuntimeError("CPU/GPU dataset metadata mismatch")
        if int(ref["pairs"])!=int(row["overlaps"]):
            raise RuntimeError(f"Pair-count difference at frame {frame}")
        if ref["pair_hash"]!=row["pair_hash"]:
            raise RuntimeError(f"PAIR SET HASH MISMATCH at frame {frame}; do not claim agreement")
        if int(row["misses"]) or int(ref["misses"]):
            raise RuntimeError(f"Oracle failure at frame {frame}")
    cpu_time=median(float(row["ms"]) for row in reference.values())
    online_time=median(float(row["ms"]) for row in online.values())
    gpu_host=median(float(row["host_ms"]) for row in gpu)
    gpu_kernel=median(float(row["gpu_ms"]) for row in gpu if float(row["gpu_ms"])>=0)
    out=(f"## Exact material CPU/Metal replay: {Path(args.metal_csv).name}\n\n"
         f"All **{len(reference)} frames matched** on collision-pair counts AND "
         f"full stable-ID pair-set FNV64 hashes. This confirms identical "
         f"candidate sets for the serialized Float32 AABB workload.\n\n"
         "| Backend | Median elapsed time | Measurement boundary |\n"
         "|---|---:|---|\n"
         f"| CPU rebuilt SAP | {cpu_time:.5f} ms | CPU complete query |\n"
         f"| CPU online material controller | {online_time:.5f} ms | CPU decision + full query |\n"
         f"| Metal GPU SAP host wall time | {gpu_host:.5f} ms | CPU sorting + Metal count/prefix/emit/readback |\n"
         f"| Metal kernel execution sum | {gpu_kernel:.5f} ms | GPU-only count + pair emit; NOT end-to-end |\n\n"
         "CPU and Metal figures measure the same serialized primitive AABBs and "
         "full candidate sets, but their implementation languages, memory "
         "representations, scheduling and CPU/GPU clocks differ. This is not "
         "a GPU implementation of the original cloth-specific online controller.\n")
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(out,encoding="utf-8")
    print(out)

if __name__=="__main__":
    main()
