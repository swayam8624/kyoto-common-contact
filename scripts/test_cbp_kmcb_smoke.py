#!/usr/bin/env python3
"""Contract and exact-pair smoke for the CBP KYOTOM01 replay adapter.

Creates only deterministic synthetic-format frames in a temporary directory.
This checks parser compatibility, parity, output schema and failure paths;
it does NOT certify scientific performance or real material trajectories.
"""
from __future__ import annotations

import csv
import pathlib
import struct
import subprocess
import sys
import tempfile


def write_snapshot(path: pathlib.Path, *, n: int = 96, frames: int = 24) -> None:
    with path.open("wb") as output:
        output.write(b"KYOTOM01")
        output.write(struct.pack("<4I", n, frames, 12001, 0))
        for frame in range(frames):
            stage = frame // 6
            for primitive in range(n):
                if stage == 0:  # Separated initial candidates
                    x = float(primitive) * 2.5
                    y = 0.0
                elif stage == 1:  # Moderate local overlap
                    x = (primitive % 12) * 0.43
                    y = (primitive // 12) * 0.43
                elif stage == 2:  # Dense, output-heavy snapshot
                    x = (primitive % 12) * 0.003
                    y = (primitive // 12) * 0.003
                else:  # Sudden motion, newly separated clusters
                    x = (primitive % 12) * 0.45 + frame * 0.09
                    y = (primitive // 12) * 0.45
                z = float(primitive % 3) * 0.02
                extent = 1.5 if stage == 2 else 0.48
                output.write(
                    struct.pack("<6f", x, y, z, x + extent, y + extent, z + extent)
                )


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_cbp_kmcb_smoke.py CBP_KMCB_BINARY")
    executable = pathlib.Path(sys.argv[1]).resolve()
    if not executable.is_file():
        raise RuntimeError(f"Missing built replay executable: {executable}")

    with tempfile.TemporaryDirectory(prefix="kyoto-cbp-kmcb-") as workdir:
        work = pathlib.Path(workdir)
        snapshot = work / "fixture.kmcb"
        trace = work / "first_trace.csv"
        write_snapshot(snapshot)

        cmd = [str(executable), str(snapshot), "32", "24", str(trace)]
        result = subprocess.run(cmd, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            raise AssertionError(
                f"KYOTOM01 replay failed:\n{result.stdout}\n{result.stderr}"
            )

        summary = list(csv.DictReader(result.stdout.splitlines()))
        if len(summary) != 6:
            raise AssertionError(f"Expected six method rows; got {len(summary)}")
        expected_names = {
            "sap_rebuild",
            "v4_guard_sap_bounded_trial",
            "region_exhaustive",
            "region_shared_bvh",
            "region_certified_cache",
            "causal_cbp",
        }
        if {row["method"] for row in summary} != expected_names:
            raise AssertionError("Missing or duplicated benchmark method")
        for row in summary:
            if row["exact_parity"] != "verified":
                raise AssertionError("Exact candidate oracle was not verified")
            if int(row["N"]) != 96 or int(row["frames"]) != 24:
                raise AssertionError("Snapshot metadata mismatch")
            if int(row["timed"]) != 21:
                raise AssertionError("Incorrect timed-frame warmup accounting")
            if float(row["mean_ms"]) <= 0:
                raise AssertionError("Invalid method elapsed time")

        trace_rows = list(csv.DictReader(trace.read_text().splitlines()))
        if len(trace_rows) != 21 * 6:
            raise AssertionError(f"Expected 126 trace rows, got {len(trace_rows)}")
        if not any(row["method"] == "causal_cbp" for row in trace_rows):
            raise AssertionError("Missing causal manager trace")

        # Overwrite protection is intentional: benchmark evidence is immutable.
        again = subprocess.run(cmd, text=True, capture_output=True, check=False)
        if again.returncode == 0:
            raise AssertionError("Evidence trace overwriting was silently allowed")

        invalid = work / "invalid.kmcb"
        invalid.write_bytes(b"KYOTOM00" + snapshot.read_bytes()[8:])
        bad = subprocess.run(
            [str(executable), str(invalid), "32", "24"],
            text=True, capture_output=True, check=False,
        )
        if bad.returncode == 0:
            raise AssertionError("Corrupt KYOTOM01 magic was accepted")

        truncated = work / "truncated.kmcb"
        truncated.write_bytes(snapshot.read_bytes()[:-3])
        short = subprocess.run(
            [str(executable), str(truncated), "32", "24"],
            text=True, capture_output=True, check=False,
        )
        if short.returncode == 0:
            raise AssertionError("Truncated snapshot was accepted")

    print(
        "PASS CBP material replay: six exact policies, 24 snapshots, "
        "126 trace rows, overwrite/corrupt/truncated rejection"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
