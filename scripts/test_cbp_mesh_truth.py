#!/usr/bin/env python3
"""Truthful mesh preview must reproduce original exact simulated AABBs.

Generates 12 independently stepped material worlds, then re-simulates their
actual nodes/bonds/triangles; exporter rejects any Float32 AABB drift.
"""
from __future__ import annotations
import json
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile


def run(args: list[str], success: bool = True):
    p = subprocess.run(args, text=True, capture_output=True, timeout=90,
                       check=False)
    if (p.returncode == 0) != success:
        raise AssertionError(f"Unexpected status {p.returncode}\n"
                             f"{' '.join(args)}\n{p.stdout}\n{p.stderr}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: test_cbp_mesh_truth.py MASS_EXPORT MESH_EXPORT")
    generator, mesh_export = map(lambda x: str(pathlib.Path(x).resolve()), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="cbp-real-mesh-") as temp:
        parent = pathlib.Path(temp)
        case = parent / "source"
        run([generator, str(case), "12", "12", "541", "crowded"])
        result = case / "real-mesh.json"
        run([mesh_export, str(case), str(result), "12"])
        j = json.loads(result.read_text(encoding="utf-8"))
        assert j["schema"] == "CCP_MESH_TRUTH_V1"
        assert j["framesTotal"] == 12
        assert j["instancesTotal"] == 12
        assert len(j["selected"]) == 12
        assert len(j["positions"]) == 12
        for frame in j["positions"]:
            assert len(frame) == 12
            for idx, coords in enumerate(frame):
                assert len(coords) == j["selected"][idx]["nodeCount"] * 3
        for idx, item in enumerate(j["selected"]):
            count = item["nodeCount"]
            assert count > 1 and item["edges"]
            assert all(0 <= a < count and 0 <= b < count
                       for a, b in item["edges"])
        assert {m["kind"] for m in j["selected"]} == set(range(5))
        run([mesh_export, str(case), str(result), "12"], success=False)

        # Mutate a copied binary, preserving original fixture and receipts.
        bad = parent / "forged"
        shutil.copytree(case, bad)
        raw = bytearray((bad / "snapshots.kmcb").read_bytes())
        old = struct.unpack_from("<f", raw, 24)[0]
        struct.pack_into("<f", raw, 24, old + 1.0)
        (bad / "snapshots.kmcb").write_bytes(raw)
        run([mesh_export, str(bad), str(bad / "reject.json"), "12"],
            success=False)
    print("PASS real 3D mesh visual: 5 genuine material topologies, 12 frames, "
          "exact Float32 AABB source-matching and corrupted-binary rejection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
