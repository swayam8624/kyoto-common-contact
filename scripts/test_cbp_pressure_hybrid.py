#!/usr/bin/env python3
"""Phase 13 exact global AABB geometry-router regression.

No scenario labels are exposed to the C++ policy (except to generate the same
deterministic source AABBs). The comparator runs separately AFTER the hybrid
run, never as shadow queries that might influence its selection.
"""
from __future__ import annotations
import csv
from pathlib import Path
import subprocess
import sys
import tempfile


def run(args: list[str], timeout=75) -> None:
    p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if p.returncode:
        raise AssertionError(
            f"Exit {p.returncode}: {' '.join(args)}\n{p.stdout}\n{p.stderr}")


def records(file: Path) -> list[dict]:
    with file.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


FIELDS = ("exact_pairs", "hash64_sum", "hash64_xor",
          "hash32_sum", "hash32_xor")


def test_scene(cpu: str, hybrid: str, root: Path,
               scenario: str, count: int, frames: int, budget: int):
    output = root / f"{scenario}-{count}-hybrid.csv"
    control = root / f"{scenario}-{count}-fixed.csv"
    flags = [scenario, str(count), str(frames), "61749",
             str(budget), "15000", "2000000"]
    run([hybrid, str(output), *flags])
    run([cpu, str(control), *flags])
    observed, reference = records(output), records(control)
    assert len(observed) == frames
    assert len(reference) == frames * 2
    for f, record in enumerate(observed):
        assert int(record["frame"]) == f
        assert record["status"] == "complete", (
            scenario, count, f, record["status"]
        )
        assert float(record["complete_query_ms"]) >= float(record["query_ms"])
        assert float(record["complete_query_ms"]) >= float(record["grid_build_ms"])
        assert record["method"] == "geometric_work_router"
        assert record["selected_backend"] in {
            "global_3d_hash_grid", "projected_x_sweep"
        }
        if count <= 1024:
            assert record["brute_oracle_checked"] == "1"
        candidates = reference[2*f:2*f+2]
        assert len(candidates) == 2
        for base in candidates:
            if base["status"] == "complete":
                for field in FIELDS:
                    assert record[field] == base[field], (
                        scenario, count, f, field,
                        record[field], base[field]
                    )
        if scenario == "all_overlap":
            assert record["analytical_count_verified"] == "1"
            assert record["selected_backend"] == "projected_x_sweep"
        if scenario == "slabs":
            assert record["analytical_count_verified"] == "1"
            assert record["selected_backend"] == "global_3d_hash_grid"
    print(f"PASS hybrid exact pair digests: {scenario}, "
          f"N={count}, frames={frames}, "
          f"modes={set(r['selected_backend'] for r in observed)}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: test_cbp_pressure_hybrid.py PRESSURE_CPU HYBRID_CPU")
    cpu, hybrid = (str(Path(x).resolve()) for x in sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="cbp-geometric-router-") as tmp:
        root = Path(tmp)
        for scenario in ("sparse", "burst", "slabs", "teleport",
                         "bridge", "all_overlap"):
            test_scene(cpu, hybrid, root, scenario, 192, 12, 3_000_000)
        # At 8,192 all-overlapping objects the grid repeats 33.55M pairs
        # across eight spatial cells. The selector must pay its estimation
        # cost, choose sweep, and enumerate all 33.55M unique pairs.
        # The old grid is deliberately capped; its INCOMPLETE digest is NOT
        # accepted as a truth source. The exhaustive X-sweep is complete.
        test_scene(cpu, hybrid, root, "all_overlap", 8192, 2, 50_000_000)
        observed = records(root / "all_overlap-8192-hybrid.csv")
        assert all(r["selected_backend"] == "projected_x_sweep" for r in observed)
        assert all(int(r["exact_pairs"]) == 33550336 for r in observed)
        assert all(int(r["grid_work_upper_bound"]) == 8*33550336
                   for r in observed)
        assert all(int(r["projected_sweep_pairs"]) == 33550336
                   for r in observed)
    print("PASS geometry-aware exact broad-phase 7-cohort regression, "
          "including adversarial all-overlap and incomplete-control distinction")


if __name__ == "__main__":
    main()
