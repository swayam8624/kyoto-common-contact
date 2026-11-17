#!/usr/bin/env python3
"""CI contract: true MultiCloth triangle-to-sheet/mesh-patch IDs, exact pairs.

A short 12-frame smoke ensures format, immutable output, and candidate parity
for seven independently stateful methods. It is not a performance gate and
does not establish solver-wide speedup.
"""
from __future__ import annotations

import collections
import csv
import pathlib
import subprocess
import sys
import tempfile

METHODS = {
    "fresh_sap", "frozen_v4", "topology_exhaustive",
    "topology_shared_bvh", "topology_certified_cache",
    "causal_cbp", "portfolio_v4_regional",
}


def run(executable: str, path: pathlib.Path, sheets: int,
        spheres: int, frames: int, span: int) -> None:
    result = subprocess.run(
        [executable, str(path), str(sheets), str(spheres),
         str(frames), str(span)],
        capture_output=True, text=True, check=False,
        timeout=110,
    )
    if result.returncode:
        raise AssertionError(
            f"Topology replay failed ({sheets}, {spheres}, {span}): "
            f"{result.stdout}\n{result.stderr}"
        )
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != frames * len(METHODS):
        raise AssertionError(f"Unexpected row count: {len(rows)}")
    by_frame: dict[int, list[dict[str, str]]] = collections.defaultdict(list)
    for row in rows:
        by_frame[int(row["frame"])].append(row)
        if row["method"] not in METHODS or row["oracle_equal"] != "1":
            raise AssertionError("Unknown method or pair disagreement")
        if int(row["primitives"]) != int(row["triangles"]) + spheres:
            raise AssertionError("Mesh triangle/sphere count mismatch")
        if int(row["patch_span"]) != span:
            raise AssertionError("Ignored topology span")
        if float(row["query_ms"]) < 0.:
            raise AssertionError("Invalid elapsed time")
        if int(row["regions"]) < sheets:
            raise AssertionError("No meaningful regional mesh partition")
    for frame in range(1, frames + 1):
        group = by_frame[frame]
        if len(group) != len(METHODS):
            raise AssertionError("Incomplete frame")
        if {entry["method"] for entry in group} != METHODS:
            raise AssertionError("Duplicate or missing backend")
        if len({entry["pairs"] for entry in group}) != 1:
            raise AssertionError("Method pair-count inconsistency")

    # No benchmark evidence is overwritten without an explicit new filename.
    overwrite = subprocess.run(
        [executable, str(path), str(sheets), str(spheres),
         str(frames), str(span)],
        capture_output=True, text=True, check=False, timeout=20,
    )
    if overwrite.returncode == 0:
        raise AssertionError("Existing topology evidence was overwritten")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_cbp_topology_replay.py EXECUTABLE")
    executable = str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="cbp-topology-") as temp:
        root = pathlib.Path(temp)
        run(executable, root / "one-sheet-span3.csv", 1, 1, 12, 3)
        run(executable, root / "two-sheet-span6.csv", 2, 1, 12, 6)
        report = root / "topology-report.html"
        analysis = subprocess.run(
            [
                sys.executable,
                str(pathlib.Path(__file__).with_name("analyze_cbp_topology.py")),
                str(root / "one-sheet-span3.csv"),
                str(root / "two-sheet-span6.csv"),
                "--html", str(report),
            ],
            capture_output=True, text=True, check=False, timeout=20,
        )
        if analysis.returncode:
            raise AssertionError(
                f"Topology reporter failed:\n{analysis.stdout}\n{analysis.stderr}"
            )
        text = report.read_text(encoding="utf-8")
        if "Portfolio decisions" not in text or "V4 / method" not in text:
            raise AssertionError("Offline research report omitted measured evidence")
    print(
        "PASS CBP topology replay: exact 7-method parity over 24 coupled "
        "cloth frames; real sheet/cell patch identities and immutable CSV"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
