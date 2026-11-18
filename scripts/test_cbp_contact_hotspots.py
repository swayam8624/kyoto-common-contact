#!/usr/bin/env python3
"""End-to-end exact collision-candidate spatial hotspot evidence contract.

Generates a short genuine stepped-material fixture. The C++ extractor's
complete candidate count/hash must match independent CPU replay on EVERY
frame; the offline viewer must reject a deliberately corrupted sidecar.
"""
from __future__ import annotations
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def run(argv: list[str], should_succeed: bool = True) -> None:
    process = subprocess.run(
        argv, capture_output=True, text=True, check=False, timeout=110
    )
    if (process.returncode == 0) != should_succeed:
        raise AssertionError(
            f"Unexpected status {process.returncode} for {argv}\n"
            f"{process.stdout}\n{process.stderr}"
        )


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(
            "Usage: test_cbp_contact_hotspots.py MASS_EXPORT MASS_REPLAY HOTSPOT_EXTRACTOR"
        )
    exported, replayed, extracted = [
        str(Path(item).resolve()) for item in sys.argv[1:]
    ]
    renderer = str(Path(__file__).with_name("render_cbp_mass_dashboard.py"))
    with tempfile.TemporaryDirectory(prefix="cbp-exact-hotspots-") as temp:
        root = Path(temp) / "materials"
        run([exported, str(root), "12", "18", "541", "crowded"])
        run([replayed, str(root / "snapshots.kmcb"),
             str(root / "instances.csv"), str(root / "cpu-core.csv"), "core"])
        location = root / "contact-hotspots.jsonl"
        cmd = [
            extracted, str(root / "snapshots.kmcb"),
            str(root / "instances.csv"), str(location), "24"
        ]
        run(cmd)
        run(cmd, should_succeed=False)  # immutable receipts must not overwrite
        with (root / "cpu-core.csv").open(newline="", encoding="utf-8") as handle:
            cpu = {
                int(row["frame"]): row for row in csv.DictReader(handle)
                if row["mode"] == "sap_rebuild"
            }
        rows = [
            json.loads(line) for line in location.read_text(encoding="utf-8").splitlines()
        ]
        if len(rows) != 18 or len(cpu) != 18:
            raise AssertionError("Missing spatial evidence frames")
        observed_cross = 0
        for frame, row in enumerate(rows):
            reference = cpu[frame]
            if (row["frame"] != frame or
                row["pairs"] != int(reference["pairs"]) or
                row["inter_pairs"] != int(reference["inter_instance_pairs"]) or
                row["pair_hash"] != reference["pair_hash"]):
                raise AssertionError("Hotspot receipt differs from CPU exact pair list")
            if row["sampled"] != len(row["hotspots"]) or row["sampled"] > 24:
                raise AssertionError("Spatial marker sampling cap violated")
            observed_cross += row["inter_pairs"]
        if observed_cross == 0:
            raise AssertionError("Crowded fixture failed to exercise real cross-instance AABB candidates")
        dashboard = root / "observatory.html"
        run([sys.executable, renderer, str(root), "--html", str(dashboard)])
        html = dashboard.read_text(encoding="utf-8")
        if '"hotspots":[' not in html or "exact cross-instance" not in html:
            raise AssertionError("Verified spatial evidence was not embedded in V2")
        # Mutate one receipt in temporary test data to prove FAIL-CLOSED load.
        rows[5]["pair_hash"] = "1234567890"
        location.write_text(
            "\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8"
        )
        run([
            sys.executable, renderer, str(root),
            "--html", str(root / "must-reject.html")
        ], should_succeed=False)
    print(
        "PASS CBP spatial hotspot receipts: exact full-frame CPU hashes, "
        "bounded deterministic markers, real cross-instance candidate points "
        "and fail-closed altered-evidence rejection"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
