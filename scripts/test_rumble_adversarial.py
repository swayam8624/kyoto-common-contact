#!/usr/bin/env python3
"""Adversarial exact-AABB parity tests for all V5 rumble algorithms.

The C++ replay performs per-frame pair hash/count matching to independent SAP
and every fifth frame cross-checks SAP against O(N^2) brute-force AABB pairs.
Test scenes include: dense overlap, boundary-touch/degenerate AABBs,
large teleports, one dirty primitive, and varied object size/spill behavior.
"""
import argparse
import math
import struct
import subprocess
import tempfile
from pathlib import Path


def make_scene(path: Path, case: str, n: int = 48, frames: int = 32):
    with path.open("wb") as dst:
        dst.write(struct.pack("<8sIIII", b"KYOTOM01", n, frames, 45991, 0))
        for frame in range(frames):
            for i in range(n):
                if case=="dense":
                    x=(i % 7)*.008 + math.sin(frame*.25+i)*.004
                    y=(i % 5)*.008
                    z=(i % 3)*.008
                    size=.15
                elif case=="touch":
                    # Exactly coincident endpoints, zero-thickness boxes,
                    # alternating duplicated center points.
                    x=float(i % 8)
                    y=float(i // 8)
                    z=0.
                    size=0. if i%2 else 1.
                elif case=="teleport":
                    # Invalidate all spatial order/safety envelopes at once.
                    x=float((i*17+frame*41)%n)*.6
                    y=float((i*5+frame*11)%19)*.2
                    z=float(frame%2)
                    size=.25
                elif case=="sparse":
                    x=float(i)*.45
                    if i==n//2:
                        x+=math.sin(frame*.7)*n*.35
                    y=0.;z=0.;size=.22
                elif case=="multiscale":
                    x=float(i%9)*.2
                    y=float(i//9)*.2
                    z=0.
                    size=2.2 if i%11==0 else (.002 if i%3 else .18)
                else:
                    raise ValueError(case)
                dst.write(struct.pack("<6f",x,y,z,x+size,y+size,z+size))


def test(replayer: Path):
    cases=("dense","touch","teleport","sparse","multiscale")
    with tempfile.TemporaryDirectory(prefix="kyoto-rumble-oracle-") as tmp:
        root=Path(tmp)
        for case in cases:
            snapshot=root/f"{case}.kmcb"
            report=root/f"{case}.csv"
            make_scene(snapshot,case)
            run=subprocess.run([str(replayer),str(snapshot),str(report),"0"],
                               capture_output=True,text=True,timeout=90)
            if run.returncode!=0:
                raise RuntimeError(f"{case} FAILED:\n{run.stdout}\n{run.stderr}")
            if "exact pair hashes matched" not in run.stdout:
                raise RuntimeError(f"{case}: absent C++ exact-pair confirmation")
            print(f"PASS {case}: exact candidate-pair parity every frame",flush=True)


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("replayer",type=Path)
    args=p.parse_args()
    test(args.replayer.resolve())
