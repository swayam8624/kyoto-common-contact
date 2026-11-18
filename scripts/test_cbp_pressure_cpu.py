#!/usr/bin/env python3
"""Cross-check adversarial C++ exactness and make capped work explicit."""
from __future__ import annotations
import csv
from pathlib import Path
import subprocess
import sys
import tempfile


def run(exe: Path, file: Path, scene: str, n: int, frames: int,
        budget: int = 10000000):
    p = subprocess.run(
        [str(exe), str(file), scene, str(n), str(frames), "61749",
         str(budget), "10000", "1000000"],
        capture_output=True, text=True, timeout=90
    )
    if p.returncode:
        raise AssertionError(p.stdout + "\n" + p.stderr)
    with file.open(newline="") as f:
        entries = list(csv.DictReader(f))
    assert len(entries) == 2 * frames
    return entries


def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="cbp-pressure-") as tmp:
        root = Path(tmp)
        for scene in ("sparse", "burst", "slabs", "teleport",
                      "bridge", "all_overlap"):
            entries = run(exe, root / (scene + ".csv"), scene, 192, 12)
            assert all(x["status"] == "complete" for x in entries)
            assert all(x["cross_method_verified"] == "1" for x in entries)
            assert all(x["brute_oracle_checked"] == "1" for x in entries)
            for frame in range(12):
                a, b = entries[frame * 2:frame * 2 + 2]
                assert a["method"] == "global_3d_hash_grid"
                assert b["method"] == "global_x_sweep"
                for attr in ("exact_pairs", "hash64_sum", "hash64_xor",
                             "hash32_sum", "hash32_xor"):
                    assert a[attr] == b[attr], (scene, frame, attr)
            if scene == "all_overlap":
                assert all(int(x["exact_pairs"]) == 192 * 191 // 2
                           for x in entries)
            print("PASS adversarial 3D global exactness", scene)
        # This is NOT an isolated island algorithm. X-axis sweeps can be
        # catastrophically quadratic in false-positive projected candidates.
        # The true 3D method still obtains an exact answer, while the
        # aborted sweep MUST be marked incomplete rather than "verified."
        entries = run(exe, root / "slabs-budget.csv",
                      "slabs", 16384, 2, 5000000)
        grid = [x for x in entries if x["method"] == "global_3d_hash_grid"]
        sweep = [x for x in entries if x["method"] == "global_x_sweep"]
        assert all(x["status"] == "complete" and
                   x["cross_method_verified"] == "0" for x in grid)
        assert all(x["status"] == "budget_stop" and
                   x["cross_method_verified"] == "0" for x in sweep)
        assert all(int(x["tested_pairs"]) == 5000000 for x in sweep)
        print("PASS truthful budgeted worst-case: complete exact global "
              "3D grid, deliberately incomplete projected sweep")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
