#!/usr/bin/env python3
"""Deterministic synthetic GLB FORMAT fixtures for real-scene importer CI.

Fixtures exercise actual glTF 2.0 BIN accessors, transforms, triangles and
the exact C++ CPU candidate oracle. Synthetic assets here are ONLY test data,
never described as downloaded Khronos source models.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


def call(args: list[str], ok=True):
    p = subprocess.run(args, capture_output=True, text=True, timeout=90)
    if (p.returncode == 0) != ok:
        raise AssertionError(
            f"Unexpected status {p.returncode}: {args}\n{p.stdout}\n{p.stderr}")


def glb_fixture(index: int) -> bytes:
    vertices = (
        (0, 0, 0), (.5, 0, 0), (.0, .5, 0),
        (.0, 0, .45), (.5, .5, .4), (.2, .2, .6)
    )
    faces = (0, 1, 2, 0, 1, 3, 1, 4, 5, 0, 2, 5)
    vb = struct.pack("<18f", *(n for v in vertices for n in v))
    ib = struct.pack("<12H", *faces)
    binary = vb + ib
    data = {
        "asset": {"version": "2.0", "generator": "Kyoto CI synthetic-only"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vb)},
            {"buffer": 0, "byteOffset": len(vb), "byteLength": len(ib)}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126,
             "count": len(vertices), "type": "VEC3"},
            {"bufferView": 1, "componentType": 5123,
             "count": len(faces), "type": "SCALAR"}
        ],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0},
                                  "indices": 1, "mode": 4}]}],
        "nodes": [{"mesh": 0, "translation": [index * .001, 0, 0]}],
        "scenes": [{"nodes": [0]}], "scene": 0
    }
    packed = json.dumps(data, separators=(",", ":")).encode("utf-8")
    packed += b" " * ((-len(packed)) % 4)
    binary += b"\0" * ((-len(binary)) % 4)
    total = 12 + 8 + len(packed) + 8 + len(binary)
    return (struct.pack("<4sII", b"glTF", 2, total)
            + struct.pack("<II", len(packed), 0x4E4F534A) + packed
            + struct.pack("<II", len(binary), 0x004E4942) + binary)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_cbp_gltf_import.py MASS_REPLAY_BINARY")
    replay = str(Path(sys.argv[1]).resolve())
    directory = Path(__file__).resolve().parent
    importer = directory / "import_cbp_gltf_scenes.py"
    renderer = directory / "render_cbp_gltf_scenes.py"
    with tempfile.TemporaryDirectory(prefix="cbp-gltf-truth-") as tmp:
        base = Path(tmp)
        catalog = base / "catalog"
        catalog.mkdir()
        downloaded = []
        for i in range(12):
            name = f"SyntheticGLBForCITest{i}"
            folder = catalog / name
            folder.mkdir()
            binary = glb_fixture(i)
            path = folder / (name + ".glb")
            path.write_bytes(binary)
            (folder / "ORIGINAL_LICENSE.md").write_text(
                "Synthetic test-only generated asset, no external copyright claim"
            )
            downloaded.append({
                "name": name,
                "file": str(path.relative_to(catalog)),
                "source": "synthetic-ci-only://" + name,
                "license_source": "test://generated",
                "sha256": hashlib.sha256(binary).hexdigest(),
            })
        manifest = {
            "schema": "CCP_GLTF_SCENE_CATALOG_V1",
            "source": "SYNTHETIC_CI_ONLY",
            "revision": "fixtures",
            "downloaded": downloaded,
        }
        (catalog / "ASSET_CATALOG.json").write_text(json.dumps(manifest))
        scene = base / "scene"
        call([sys.executable, str(importer), "--catalog", str(catalog),
              "--output", str(scene), "--frames", "12",
              "--max-triangles-per-asset", "4", "--layout", "crowded"])
        data = json.loads((scene / "asset-meshes.json").read_text())
        assert len(data["objects"]) == 12
        assert all(x["source_triangles"] == 4 for x in data["objects"])
        assert all(x["display_triangles"] == 4 and
                   len(x["visual_triangles"]) == 4
                   for x in data["objects"])
        assert all(x["selected_triangles"] == 4 and
                   len(x["triangles"]) == 4
                   for x in data["objects"])
        assert len(data["frames"]) == 12
        call([replay, str(scene / "snapshots.kmcb"),
              str(scene / "instances.csv"), str(scene / "cpu-core.csv"), "core"])
        html = scene / "viewer.html"
        call([sys.executable, str(renderer), str(scene), "--html", str(html)])
        page = html.read_text()
        for token in ("Real 3D Scene", "gl.drawArrays(gl.TRIANGLES",
                      "CPU / broad-phase evidence", "synthetic-ci-only",
                      "Higher-detail GLB display", "Measured collision triangles",
                      "Collision-query triangles"):
            if token.lower() not in page.lower():
                raise AssertionError("Actual mesh viewer contract missing " + token)
        node = shutil.which("node")
        if node:
            import re
            chunks = re.findall(r"<script>(.*?)</script>", page, flags=re.S)
            assert len(chunks) == 1
            client = base / "browser.js"
            client.write_text(chunks[0])
            call([node, "--check", str(client)])
        # Display-mesh corruption must fail closed before browser output.
        broken = json.loads((scene / "asset-meshes.json").read_text())
        broken["objects"][0]["visual_triangles"][0][0] = "fake"
        (scene / "asset-meshes.json").write_text(json.dumps(broken))
        call([sys.executable, str(renderer), str(scene),
              "--html", str(scene / "forged-visual.html")], ok=False)
        # Copy one source and tamper with it, ensuring provenance closes.
        filename = catalog / downloaded[0]["file"]
        filename.write_bytes(filename.read_bytes()[:-3] + b"xxx")
        call([sys.executable, str(importer), "--catalog", str(catalog),
              "--output", str(base / "must_reject"), "--frames", "12"],
             ok=False)
    print("PASS GLB import/render format: twelve unique generated GLB mesh "
          "fixtures, true per-triangle AABB replay, exact full CPU pair oracle, "
          "offline WebGL syntax, source hash tamper rejection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
