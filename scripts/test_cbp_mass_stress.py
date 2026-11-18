#!/usr/bin/env python3
"""Small CI contract for hundreds/thousands-world stress *infrastructure*.

Runs 12 REAL independently stepped material Worlds and all eight CPU
algorithms. Full candidate parity is checked by C++; this harness also
recomputes a separate Python overlap oracle and stable-ID FNV hash on
selected binary frames, checks fixed topology, output integrity and guards
against silently overwriting an existing experiment.
"""
from __future__ import annotations
import csv
import pathlib
import struct
import subprocess
import sys
import tempfile
from collections import defaultdict

EXPECTED = {
    "sap_rebuild", "frozen_v4", "instance_exhaustive", "instance_xsweep",
    "instance_shared_bvh", "instance_certified_cache", "causal_cbp",
    "portfolio_v4_regional",
}
MATERIALS = {"cloth", "sponge", "cushion", "soft_ball", "spring"}


def cmd(arguments: list[str], success: bool = True) -> None:
    result = subprocess.run(
        arguments, text=True, capture_output=True, timeout=85,
        check=False,
    )
    if (result.returncode == 0) != success:
        raise AssertionError(
            f"Unexpected command status {result.returncode}:\n"
            f"{' '.join(arguments)}\n{result.stdout}\n{result.stderr}"
        )


def check_binary(path: pathlib.Path, n: int, frames: int) -> None:
    with path.open("rb") as handle:
        header = handle.read(24)
        magic, count, total_frames, seed, material = struct.unpack(
            "<8sIIII", header
        )
        if (magic, count, total_frames, seed, material) != (
            b"KYOTOM01", n, frames, 541, 5
        ):
            raise AssertionError("Invalid shared CPU/Metal binary header")
        if path.stat().st_size != 24 + n * frames * 24:
            raise AssertionError("Invalid binary length")
        first = handle.read(n * 24)
        handle.seek(24 + n * (frames - 1) * 24)
        last = handle.read(n * 24)
        if first == last:
            raise AssertionError("The stepped real-material geometry never moved")
        # Independent inclusive AABB pair oracle, from actual stored f32 data.
        for frame, raw in ((0, first), (frames - 1, last)):
            boxes = list(struct.iter_unpack("<6f", raw))
            if len(boxes) != n:
                raise AssertionError("Truncated snapshot")
            pairs = [
                (i, j)
                for i in range(n)
                for j in range(i + 1, n)
                if all(
                    boxes[i][axis] <= boxes[j][axis + 3] and
                    boxes[j][axis] <= boxes[i][axis + 3]
                    for axis in range(3)
                )
            ]
            h = 14695981039346656037
            for i, j in pairs:
                for value in (i, j):
                    h = (h ^ value) * 1099511628211 & 0xFFFFFFFFFFFFFFFF
            yield frame, len(pairs), h


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "Usage: test_cbp_mass_stress.py MASS_EXPORT_BINARY MASS_REPLAY_BINARY"
        )
    export = str(pathlib.Path(sys.argv[1]).resolve())
    replay = str(pathlib.Path(sys.argv[2]).resolve())
    with tempfile.TemporaryDirectory(prefix="cbp-manyworld-ci-") as temp:
        directory = pathlib.Path(temp) / "many-materials"
        cmd([export, str(directory), "12", "12", "541", "contact_front"])
        cmd([export, str(directory), "12", "12", "541", "contact_front"],
            success=False)
        for required in (
            "snapshots.kmcb", "instances.csv", "preview.jsonl", "frames.csv"
        ):
            if not (directory / required).is_file():
                raise AssertionError(f"Missing shared evidence {required}")
        with (directory / "instances.csv").open(newline="") as f:
            instances = list(csv.DictReader(f))
        if len(instances) != 12 or (
            {x["kind"] for x in instances} != MATERIALS
        ):
            raise AssertionError("Mixed 5-class material scene not generated")
        coverage = 0
        for identity, x in enumerate(instances):
            if int(x["id"]) != identity or int(x["first_primitive"]) != coverage:
                raise AssertionError("Instance ranges are not contiguous")
            coverage += int(x["primitive_count"])
        receipt = list(csv.DictReader(
            (directory / "frames.csv").read_text().splitlines()
        ))
        if len(receipt) != 12 or {int(x["primitives"]) for x in receipt} != {coverage}:
            raise AssertionError("Recorded snapshot metadata differs")
        preview = (directory / "preview.jsonl").read_text().splitlines()
        if len(preview) != 12:
            raise AssertionError("Visual frames do not match binary snapshot")
        live = directory / "preview-geometry-only.html"
        cmd([
            sys.executable,
            str(pathlib.Path(__file__).with_name("render_cbp_mass_dashboard.py")),
            str(directory), "--preview-only", "--html", str(live),
        ])
        if "BENCHMARK EXECUTING" not in live.read_text(encoding="utf-8"):
            raise AssertionError("Live view falsely asserted measured benchmark results")

        report = directory / "cpu-full.csv"
        cmd([replay, str(directory / "snapshots.kmcb"),
             str(directory / "instances.csv"), str(report), "full"])
        cmd([replay, str(directory / "snapshots.kmcb"),
             str(directory / "instances.csv"), str(report), "full"],
            success=False)
        with report.open(newline="") as f:
            rows = list(csv.DictReader(f))
        if len(rows) != 12 * 8:
            raise AssertionError(f"Expected 96 method-frame rows; saw {len(rows)}")
        frame_rows: dict[int, list[dict[str, str]]] = defaultdict(list)
        for row in rows:
            frame_rows[int(row["frame"])].append(row)
            if row["exact_sap_equal"] != "1":
                raise AssertionError("One algorithm failed complete pair equality")
        for frame in range(12):
            selected = frame_rows[frame]
            if {x["mode"] for x in selected} != EXPECTED:
                raise AssertionError(f"Missing or duplicate mode at frame {frame}")
            if len({x["pairs"] for x in selected}) != 1:
                raise AssertionError("Missing cross-method pair equality")
            if len({x["pair_hash"] for x in selected}) != 1:
                raise AssertionError("Inconsistent full sorted-pair hashes")
        for frame, size, digest in check_binary(
            directory / "snapshots.kmcb", coverage, 12
        ):
            expected = frame_rows[frame][0]
            if int(expected["pairs"]) != size or int(expected["pair_hash"]) != digest:
                raise AssertionError(
                    "Python independent float32 AABB oracle or FNV hash mismatch"
                )
        dashboard = directory / "interactive.html"
        cmd([
            sys.executable,
            str(pathlib.Path(__file__).with_name("render_cbp_mass_dashboard.py")),
            str(directory), "--html", str(dashboard),
        ])
        html = dashboard.read_text(encoding="utf-8")
        if ("gl.drawArraysInstanced" not in html or
            "CPU/Metal full pair counts" not in html or
            "mass-material" not in html.lower()):
            raise AssertionError("Offline WebGL dashboard missing evidence or graphics")
        cmd([
            sys.executable,
            str(pathlib.Path(__file__).with_name("render_cbp_mass_dashboard.py")),
            str(directory), "--html", str(dashboard),
        ], success=False)
    print(
        "PASS CBP 12 independently stepped material worlds, 5 actual "
        "material families, exact binary CPU/Metal contract, 8 methods, "
        "full pair parity and independent Python F32 pair/hash oracles"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
