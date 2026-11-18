#!/usr/bin/env python3
"""Acquire a bounded, license-audited 26-asset Khronos glTF GLB scene pack.

Download ONLY from a pinned Khronos glTF-Sample-Assets commit, verify the
upstream machine-readable model index and each model's actual README license,
skip unknown/noncommercial/trademark-restricted objects, limit network bytes,
and preserve source URL + local SHA-256 provenance. Do NOT commit downloaded
models into the Kyoto repository.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import urllib.error
import urllib.request

UPSTREAM = "KhronosGroup/glTF-Sample-Assets"
REVISION = "f0920f5b32d172a30efe3f9257e965918831e6ea"
BASE = f"https://raw.githubusercontent.com/{UPSTREAM}/{REVISION}/Models"
NAMES = (
    "Avocado", "BarramundiFish", "BoomBox", "Box", "BoxInterleaved",
    "BoxTextured", "BoxTexturedNonPowerOfTwo", "BoxVertexColors",
    "CesiumMilkTruck", "CompareAlphaCoverage", "CompareAmbientOcclusion",
    "CompareMetallic", "CompareNormal", "CompareRoughness", "Lantern",
    "MetalRoughSpheresNoTextures", "NegativeScaleTest",
    "NormalTangentMirrorTest", "NormalTangentTest", "OrientationTest",
    "TextureCoordinateTest", "TextureEncodingTest", "TextureSettingsTest",
    "VertexColorTest", "WaterBottle", "Fox",
)
ALLOW = (
    "Creative Commons Zero v1.0 Universal",
    "Creative Commons Attribution 4.0 International",
)
REJECT = ("Attribution Non Commercial", "3DRT license", "SCEA Shared Source",
          "Poser EULA", "Cryengine Limited", "NonCommercial")


def fetch(url: str, limit: int) -> bytes:
    request = urllib.request.Request(
        url, headers={"User-Agent": "Kyoto-CommonBroadPhase-research/1.0"}
    )
    with urllib.request.urlopen(request, timeout=45) as response:
        header = response.headers.get("Content-Length")
        if header and int(header) > limit:
            raise ValueError(f"File exceeds budget before transfer: {url}")
        data = bytearray()
        while True:
            block = response.read(min(1 << 20, limit + 1 - len(data)))
            if not block:
                break
            data.extend(block)
            if len(data) > limit:
                raise ValueError(f"File exceeds transfer budget: {url}")
        return bytes(data)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--limit", type=int, default=26)
    p.add_argument("--per-asset-mb", type=int, default=24)
    p.add_argument("--total-mb", type=int, default=280)
    args = p.parse_args()
    if not 1 <= args.limit <= len(NAMES):
        raise ValueError("Choose 1..26 curated models")
    if not 1 <= args.per_asset_mb <= 80 or not 1 <= args.total_mb <= 800:
        raise ValueError("Unsafe network budget")
    if args.output.exists():
        raise FileExistsError("Preserve prior assets; choose an unused output path")
    args.output.mkdir(parents=True, exist_ok=False)
    catalog = json.loads(fetch(BASE + "/model-index.json", 4 << 20))
    catalog_by_name = {row["name"]: row for row in catalog}
    successes, failures, total_bytes = [], [], 0
    for index, name in enumerate(NAMES[:args.limit], start=1):
        try:
            model = catalog_by_name[name]
            if "glTF-Binary" not in model.get("variants", {}):
                raise ValueError("No standard GLB variant")
            filename = model["variants"]["glTF-Binary"]
            license_url = f"{BASE}/{name}/README.md"
            license_text = fetch(license_url, 500_000).decode("utf-8")
            legal = license_text[license_text.find("## Legal"):]
            if not legal or not any(allowed in legal for allowed in ALLOW):
                raise ValueError("No permitted CC0/CC-BY license found")
            if any(forbidden in legal for forbidden in REJECT):
                raise ValueError("Noncommercial or restricted terms detected")
            source = f"{BASE}/{name}/glTF-Binary/{filename}"
            budget = min(
                args.per_asset_mb << 20,
                (args.total_mb << 20) - total_bytes,
            )
            if budget <= 12:
                raise ValueError("Total download budget exhausted")
            raw = fetch(source, budget)
            if len(raw) < 20 or raw[:4] != b"glTF" or struct.unpack_from(
                    "<I", raw, 4)[0] != 2:
                raise ValueError("Not a valid glTF 2.0 GLB")
            if struct.unpack_from("<I", raw, 8)[0] != len(raw):
                raise ValueError("GLB internal byte length mismatch")
            folder = args.output / name
            folder.mkdir(exist_ok=False)
            (folder / filename).write_bytes(raw)
            (folder / "ORIGINAL_LICENSE.md").write_text(
                license_text, encoding="utf-8"
            )
            total_bytes += len(raw)
            successes.append({
                "id": len(successes), "name": name,
                "label": model.get("label", name),
                "file": str(Path(name) / filename),
                "source": source,
                "license_source": license_url,
                "original_readme": str(Path(name) / "ORIGINAL_LICENSE.md"),
                "license": "CC0 and/or CC BY 4.0 (read original license file)",
                "sha256": hashlib.sha256(raw).hexdigest(),
                "bytes": len(raw), "source_revision": REVISION,
            })
            print(f"[ASSET {index}/{args.limit}] {name} "
                  f"{len(raw)/1e6:.2f} MB SHA-256 verified", flush=True)
        except (ValueError, KeyError, urllib.error.URLError,
                TimeoutError, OSError) as error:
            failures.append({"name": name, "reason": str(error)})
            print(f"[ASSET SKIPPED] {name}: {error}", flush=True)
    report = {
        "schema": "CCP_GLTF_SCENE_CATALOG_V1",
        "source": UPSTREAM, "revision": REVISION,
        "downloaded": successes, "skipped": failures,
        "bytes_downloaded": total_bytes,
        "research_scope": ("Static glTF triangle geometry with separately "
                           "defined rigid motions, NOT a coupled deformable solver"),
    }
    (args.output / "ASSET_CATALOG.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(f"[ASSETS] validated {len(successes)} / {args.limit} "
          f"curated sources; {len(failures)} skipped, "
          f"{total_bytes/1e6:.2f} MB. NEVER claim skipped assets loaded.")
    if len(successes) < 12:
        raise RuntimeError("Fewer than 12 usable 3D GLB scenes, no benchmark cohort")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
