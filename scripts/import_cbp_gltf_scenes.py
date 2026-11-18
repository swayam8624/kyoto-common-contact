#!/usr/bin/env python3
"""Convert a license-audited Khronos GLB set into moving REAL TRIANGLE AABBs.

This is NOT a deformable solver or GLB animation playback. Static triangles
from each imported core glTF mesh are transformed rigidly each frame and fed
unchanged to the existing EXACT CPU + Apple Metal KYOTOM01 replay services.
Real source triangle vertices, per-model selection fractions, and rigid
motions are saved for a separate truthful browser viewer.

Unsupported glTF extensions/accessors fail closed per asset (with reasons).
Input SHA-256 and real source provenance are verified before parsing.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


def glb(path: Path) -> tuple[dict, bytes]:
    raw = path.read_bytes()
    if len(raw) < 20 or raw[:4] != b"glTF":
        raise ValueError("Invalid glTF binary file")
    _, version, total = struct.unpack_from("<4sII", raw)
    if version != 2 or total != len(raw):
        raise ValueError("Invalid glTF binary version/length")
    at = 12
    chunks = []
    while at < len(raw):
        if at + 8 > len(raw):
            raise ValueError("Truncated GLB chunk header")
        n, ctype = struct.unpack_from("<II", raw, at)
        at += 8
        if n > len(raw) - at:
            raise ValueError("Truncated GLB chunk data")
        chunks.append((ctype, raw[at:at + n]))
        at += n
    if not chunks or chunks[0][0] != 0x4E4F534A:
        raise ValueError("GLB JSON chunk missing")
    document = json.loads(chunks[0][1].decode("utf-8"))
    binary = next((chunk for code, chunk in chunks if code == 0x004E4942), b"")
    if not binary:
        raise ValueError("Missing embedded glTF geometry BIN chunk")
    if document.get("extensionsRequired"):
        raise ValueError("Unsupported required glTF extensions: "
                         + ", ".join(document["extensionsRequired"]))
    return document, binary


def accessor(document: dict, buffer: bytes, index: int) -> list:
    item = document["accessors"][index]
    if "sparse" in item:
        raise ValueError("Sparse accessor not supported in exact triangle importer")
    types = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    components = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4),
                  5126: ("f", 4)}
    kind = item["type"]
    if kind not in types or item["componentType"] not in components:
        raise ValueError("Unsupported vertex/index accessor")
    dim = types[kind]
    fmt, width = components[item["componentType"]]
    view = document["bufferViews"][item["bufferView"]]
    stride = view.get("byteStride", dim * width)
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    if view.get("buffer", 0) != 0 or stride < dim * width:
        raise ValueError("External GLB buffer or illegal stride")
    count = item["count"]
    if not 0 < count <= 8_000_000 or start < 0:
        raise ValueError("Vertex count/offset exceeds import budget")
    if start + (count - 1) * stride + dim * width > len(buffer):
        raise ValueError("Accessor overruns source binary")
    unpack = struct.Struct("<" + fmt * dim).unpack_from
    result = [unpack(buffer, start + k * stride) for k in range(count)]
    return [values[0] if dim == 1 else values for values in result]


def identity() -> list[float]:
    return [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]


def matmul(a: list[float], b: list[float]) -> list[float]:
    # Column-major glTF matrices: result=a@b.
    return [sum(a[row + 4 * k] * b[k + 4 * col] for k in range(4))
            for col in range(4) for row in range(4)]


def local_matrix(node: dict) -> list[float]:
    if "matrix" in node:
        if len(node["matrix"]) != 16:
            raise ValueError("Invalid glTF node transform")
        return [float(v) for v in node["matrix"]]
    tx, ty, tz = node.get("translation", [0, 0, 0])
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    sx, sy, sz = node.get("scale", [1, 1, 1])
    rot = [
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w),
        2 * (x * z - y * w), 0.,
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z),
        2 * (y * z + x * w), 0.,
        2 * (x * z + y * w), 2 * (y * z - x * w),
        1 - 2 * (x * x + y * y), 0.,
        tx, ty, tz, 1.,
    ]
    for c, scale in enumerate((sx, sy, sz)):
        for r in range(3):
            rot[r + c * 4] *= scale
    return rot


def point(m: list[float], xyz: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = xyz
    w = m[3] * x + m[7] * y + m[11] * z + m[15]
    if abs(w) < 1e-12:
        raise ValueError("Degenerate glTF homogeneous world position")
    return ((m[0] * x + m[4] * y + m[8] * z + m[12]) / w,
            (m[1] * x + m[5] * y + m[9] * z + m[13]) / w,
            (m[2] * x + m[6] * y + m[10] * z + m[14]) / w)


def triangles(document: dict, binary: bytes) -> list[tuple]:
    result = []
    nodes = document.get("nodes", [])
    scene_id = document.get("scene", 0)
    scenes = document.get("scenes", [])
    roots = scenes[scene_id]["nodes"] if scenes else list(range(len(nodes)))
    mesh_visited = 0

    def visit(index: int, parent: list[float], parents: set[int]):
        nonlocal mesh_visited
        if index in parents:
            raise ValueError("Cyclic glTF node hierarchy")
        node = nodes[index]
        transform = matmul(parent, local_matrix(node))
        if "skin" in node:
            raise ValueError("Skinned GLB needs a skeletal pose; skipping asset")
        if "mesh" in node:
            mesh_visited += 1
            for primitive in document["meshes"][node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    continue
                if "POSITION" not in primitive["attributes"]:
                    continue
                vertices = accessor(document, binary,
                                    primitive["attributes"]["POSITION"])
                if (not all(isinstance(v, tuple) and len(v) == 3
                            for v in vertices)):
                    raise ValueError("POSITION accessor is not VEC3")
                ids = accessor(document, binary, primitive["indices"]) \
                    if "indices" in primitive else list(range(len(vertices)))
                if len(ids) % 3:
                    raise ValueError("glTF triangle index count not multiple of 3")
                for j in range(0, len(ids), 3):
                    triple = [point(transform, vertices[ids[j + k]])
                              for k in range(3)]
                    if not all(math.isfinite(c) for v in triple for c in v):
                        raise ValueError("Nonfinite decoded glTF triangle")
                    result.append(tuple(c for v in triple for c in v))
                    if len(result) > 1_500_000:
                        raise ValueError("Model has too many source triangles")
        for child in node.get("children", []):
            visit(child, transform, parents | {index})

    for root in roots:
        visit(root, identity(), set())
    if not result or mesh_visited == 0:
        raise ValueError("Asset has no supported indexed/core static triangles")
    return result


def compact(source: list[tuple], limit: int) -> tuple[list[list[float]], int]:
    count = len(source)
    picks = [source[min(count - 1, math.floor(k * count / min(count, limit)))]
             for k in range(min(count, limit))]
    coords = [value for tri in picks for value in tri]
    lo = [min(coords[k::3]) for k in range(3)]
    hi = [max(coords[k::3]) for k in range(3)]
    extent = max(hi[k] - lo[k] for k in range(3))
    if extent < 1e-10:
        raise ValueError("Degenerate sampled geometry")
    center = [(lo[k] + hi[k]) / 2 for k in range(3)]
    scale = 1.55 / extent
    return [[round((tri[i] - center[i % 3]) * scale, 6)
             for i in range(9)] for tri in picks], count


def placement(i: int, frame: int, frames: int, count: int, layout: str):
    # 8 assets per cluster; gradual rigid convergence is a workload trajectory
    # only, NOT collision response or glTF skeleton/animation evaluation.
    group, member = divmod(i, 8)
    groups = math.ceil(count / 8)
    side = math.ceil(groups ** (1 / 3))
    gx = group % side
    gy = (group // side) % side
    gz = group // (side * side)
    t = frame / max(1, frames - 1)
    local = .27 if layout == "crowded" else (
        2.1 - 1.83 * max(0., min(1., (t - .10) / .70)))
    return (6.5 * (gx - side / 2) + (local if member & 1 else -local),
            6.5 * (gy - side / 2) + (local if member & 2 else -local),
            6.5 * gz + (local if member & 4 else -local),
            .17 * math.sin(frame * .04 + i * .13))


def move(v: list[float], pose: tuple[float, float, float, float]):
    dx, dy, dz, angle = pose
    c, s = math.cos(angle), math.sin(angle)
    x, y, z = v
    return (float(c * x - s * y + dx),
            float(s * x + c * y + dy),
            float(z + dz))


def generate(catalog: Path, output: Path, frames: int, max_tris: int,
             seed: int, layout: str):
    if output.exists():
        raise FileExistsError("Never overwrite prior asset-triangle replay evidence")
    raw_catalog = json.loads((catalog / "ASSET_CATALOG.json").read_text())
    if raw_catalog.get("schema") != "CCP_GLTF_SCENE_CATALOG_V1":
        raise ValueError("Source catalog is not an audited Khronos asset pack")
    assets = []
    for model in raw_catalog["downloaded"]:
        path = catalog / model["file"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != model["sha256"]:
            raise ValueError(f"Source GLB digest changed: {model['name']}")
        try:
            document, buffer = glb(path)
            tris, original_count = compact(triangles(document, buffer), max_tris)
            assets.append({
                "name": model["name"], "source": model["source"],
                "license_source": model["license_source"],
                "sha256": model["sha256"], "source_triangles": original_count,
                "selected_triangles": len(tris), "triangles": tris,
            })
            print(f"[GLB SCENE] {model['name']} {len(tris)}/{original_count} "
                  f"real triangles", flush=True)
        except (KeyError, IndexError, ValueError, TypeError,
                struct.error) as e:
            print(f"[GLB SKIPPED] {model['name']} {e}", flush=True)
    if len(assets) < 12:
        raise ValueError("Fewer than twelve validated static glTF scene geometries")
    n = sum(a["selected_triangles"] for a in assets)
    if n > 65536:
        raise ValueError("Real mesh triangles exceed shared exact replay cap")
    output.mkdir(parents=True, exist_ok=False)
    with (output / "instances.csv").open("w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["id", "kind", "regime", "first_primitive", "primitive_count"])
        start = 0
        for i, asset in enumerate(assets):
            writer.writerow([i, asset["name"], f"rigid_{layout}",
                             start, asset["selected_triangles"]])
            start += asset["selected_triangles"]
    preview = (output / "preview.jsonl").open("w", encoding="utf-8")
    receipt = (output / "frames.csv").open("w", newline="")
    csv_out = csv.writer(receipt)
    csv_out.writerow(["frame", "instances", "primitives", "layout", "seed",
                      "geometry_fnv64"])
    motion = []
    with (output / "snapshots.kmcb").open("wb") as binary:
        binary.write(struct.pack("<8sIIII", b"KYOTOM01", n, frames, seed, 5))
        for f in range(frames):
            frame_boxes = []
            envelopes = []
            poses = []
            h = 14695981039346656037
            for i, asset in enumerate(assets):
                pose = placement(i, f, frames, len(assets), layout)
                poses.append([round(v, 6) for v in pose])
                lo = [float("inf")] * 3
                hi = [-float("inf")] * 3
                for tri in asset["triangles"]:
                    v = [move(tri[j:j + 3], pose) for j in (0, 3, 6)]
                    bounds = [min(p[k] for p in v) for k in range(3)] + [
                        max(p[k] for p in v) for k in range(3)]
                    packing = struct.pack("<6f", *bounds)
                    frame_boxes.append(packing)
                    for k in range(3):
                        x, y = struct.unpack_from("<f", packing, 4 * k)[0], \
                               struct.unpack_from("<f", packing, 4 * (k + 3))[0]
                        lo[k] = min(lo[k], x)
                        hi[k] = max(hi[k], y)
                envelopes.append([i % 5] + [round(x, 5) for x in lo + hi])
            for raw in frame_boxes:
                binary.write(raw)
                for (word,) in struct.iter_unpack("<I", raw):
                    h = (h ^ word) * 1099511628211 & 0xFFFFFFFFFFFFFFFF
            csv_out.writerow([f, len(assets), n, layout, seed, format(h, "x")])
            preview.write(json.dumps({"frame": f, "instances": envelopes},
                                     separators=(",", ":")) + "\n")
            motion.append(poses)
            if (f + 1) % max(1, frames // 8) == 0:
                print(f"[GLB MOTION] {f + 1}/{frames} {len(assets)} sources, "
                      f"{n} exact triangle AABBs", flush=True)
    preview.close()
    receipt.close()
    (output / "asset-meshes.json").write_text(
        json.dumps({
            "schema": "CCP_REAL_GLTF_MOTION_V1",
            "asset_source": raw_catalog["source"],
            "asset_revision": raw_catalog["revision"],
            "objects": assets,
            "frames": motion,
            "notes": ("Actual static triangle geometry from Khronos GLB assets, "
                      "normalised and moved rigidly; NO glTF animation/skin or "
                      "deformable inter-object contact dynamics"),
        }, separators=(",", ":")), encoding="utf-8"
    )
    print(f"PASS real GLB scene tournament input: scenes={len(assets)} "
          f"triangle AABBs={n} frames={frames} identity="
          f"{hashlib.sha256((output/'snapshots.kmcb').read_bytes()).hexdigest()}")
    return len(assets)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--catalog", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--frames", type=int, default=60)
    p.add_argument("--max-triangles-per-asset", type=int, default=512)
    p.add_argument("--layout", choices=("crowded", "wave"), default="wave")
    p.add_argument("--seed", type=int, default=66217)
    a = p.parse_args()
    if not 12 <= a.frames <= 240 or not 1 <= a.max_triangles_per_asset <= 2000:
        raise ValueError("Unsafe workload dimensions")
    if not 1 <= a.seed <= 2147483647:
        raise ValueError("Seed outside binary uint32 limit")
    generate(a.catalog, a.output, a.frames,
             a.max_triangles_per_asset, a.seed, a.layout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
