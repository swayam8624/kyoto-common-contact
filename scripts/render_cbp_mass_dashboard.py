#!/usr/bin/env python3
"""Offline WebGL and optional looping GIF for exact mixed-material CPU/Metal runs.

Reads actual mass-spring envelopes and measured algorithm CSVs from the same
immutable KYOTOM01 binary. A GPU result is admitted ONLY when its complete
framewise stable-ID pair hashes/counts match the CPU SAP reference.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics

MATERIALS = ("cloth", "sponge", "cushion", "soft_ball", "spring")
PALETTE = ("#4de0c7", "#7db2ff", "#f7aa6b", "#c29bff", "#ff82ad")


def records(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as file:
        result = list(csv.DictReader(file))
    if not result:
        raise ValueError("Missing CSV rows: " + str(path))
    return result


def sha256(path: Path) -> str:
    hash_value = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(4 << 20), b""):
            hash_value.update(block)
    return hash_value.hexdigest()


def load(directory: Path, metal_files: list[Path], preview_only: bool = False) -> dict:
    instances = records(directory / "instances.csv")
    receipt = records(directory / "frames.csv")
    with (directory / "preview.jsonl").open(encoding="utf-8") as file:
        frames = [json.loads(line) for line in file if line.strip()]
    if not frames or len(frames) != len(receipt):
        raise ValueError("Preview frames must match material geometry receipts")
    if {i["kind"] for i in instances} != set(MATERIALS):
        raise ValueError("All five material types must appear")
    for frame, data in enumerate(frames):
        if data["frame"] != frame or len(data["instances"]) != len(instances):
            raise ValueError("Preview missing or reordered an instance")
        for meta, box in zip(instances, data["instances"], strict=True):
            if len(box) != 7 or box[0] != MATERIALS.index(meta["kind"]):
                raise ValueError("Inconsistent material instance identity")
            if any(not math.isfinite(float(x)) for x in box[1:]):
                raise ValueError("Nonfinite visual material AABB")
            if any(box[i] > box[i + 3] for i in (1, 2, 3)):
                raise ValueError("Inverted visual material AABB")
    n = int(receipt[0]["primitives"])
    if (directory / "snapshots.kmcb").stat().st_size != 24 + 24 * n * len(frames):
        raise ValueError("Preview and binary frame data have different lengths")
    cpu = directory / "cpu-core.csv"
    if (directory / "cpu-full.csv").is_file():
        cpu = directory / "cpu-full.csv"
    if not preview_only and not cpu.is_file():
        raise ValueError("Run ccp_cbp_mass_replay to produce cpu-core.csv first")
    samples = []
    for row in ([] if preview_only else records(cpu)):
        samples.append({
            "frame": int(row["frame"]),
            "mode": row["mode"],
            "host_ms": float(row["ms"]),
            "gpu_ms": None,
            "pairs": int(row["pairs"]),
            "hash": int(row["pair_hash"]),
            "gpu": False,
        })
        if row["exact_sap_equal"] != "1":
            raise ValueError("CPU broad-phase parity was not verified")
    for path in ([] if preview_only else metal_files):
        for row in records(path):
            if int(row["misses"]) != 0:
                raise ValueError(f"Metal reported oracle misses: {path}")
            samples.append({
                "frame": int(row["frame"]),
                "mode": row["backend"],
                "host_ms": float(row["host_ms"]),
                "gpu_ms": float(row["gpu_ms"]),
                "pairs": int(row["overlaps"]),
                "hash": int(row["pair_hash"]),
                "gpu": True,
            })
    by_frame: list[dict] = [{} for _ in frames]
    for row in samples:
        index = row["frame"]
        if index < 0 or index >= len(frames):
            raise ValueError("Result frame outside recorded geometry")
        if row["mode"] in by_frame[index]:
            raise ValueError("Duplicate method/frame: " + row["mode"])
        if not math.isfinite(row["host_ms"]) or row["host_ms"] < 0:
            raise ValueError("Invalid measured host wall time")
        by_frame[index][row["mode"]] = row
    method_names = set(by_frame[0])
    if not preview_only and "sap_rebuild" not in method_names:
        raise ValueError("SAP exact reference missing from CPU benchmark")
    for index, group in enumerate(by_frame):
        if set(group) != method_names:
            raise ValueError("Incomplete CPU/GPU frame " + str(index))
        reference = group.get("sap_rebuild")
        for method, result in group.items():
            if (result["pairs"], result["hash"]) != (
                reference["pairs"], reference["hash"]
            ):
                raise ValueError(
                    "CPU/GPU exact-pair mismatch at frame "
                    + str(index) + ", method " + method
                )
    warmup = min(8, len(frames) // 8)
    stats = []
    for method in sorted(method_names):
        sorted_times = sorted(
            by_frame[f][method]["host_ms"]
            for f in range(warmup, len(frames))
        )
        stats.append({
            "mode": method,
            "mean": statistics.fmean(sorted_times),
            "median": statistics.median(sorted_times),
            "p95": sorted_times[max(0, math.ceil(.95 * len(sorted_times)) - 1)],
            "gpu": by_frame[0][method]["gpu"],
        })
    lo = [min(shape[k] for frame in frames for shape in frame["instances"])
          for k in (1, 2, 3)]
    hi = [max(shape[k] for frame in frames for shape in frame["instances"])
          for k in (4, 5, 6)]
    return {
        "frames": frames,
        "runs": by_frame,
        "stats": stats,
        "instances": len(instances),
        "primitives": n,
        "seed": int(receipt[0]["seed"]),
        "layout": receipt[0]["layout"],
        "warmup": warmup,
        "gpu_checks": sum(bool(row["gpu"]) for row in samples),
        "preview_only": preview_only,
        "limits": [lo, hi],
    }


def animated_gif(model: dict, output: Path, max_frames: int = 60) -> None:
    from PIL import Image, ImageDraw, ImageFont

    if output.exists():
        raise FileExistsError("Existing GIF evidence would be overwritten")
    total = len(model["frames"])
    selected = sorted({round(i * (total - 1) / max(1, min(max_frames, total) - 1))
                       for i in range(min(max_frames, total))})
    lo, hi = model["limits"]
    w, h = 1050, 590
    sc = min(680 / max(1.0, hi[0] - lo[0]),
             480 / max(1.0, hi[1] - lo[1]))
    middle = [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2]
    images = []
    for index in selected:
        image = Image.new("RGB", (w, h), "#0b1320")
        pen = ImageDraw.Draw(image)
        pen.text((22, 20), "KYOTO CBP / MIXED MATERIAL MASS STRESS", fill="#e0edf7")
        pen.text((22, 43),
                 f"Frame {index + 1}/{total}  |  {model['instances']} independent "
                 f"material worlds  |  {model['primitives']} element AABBs",
                 fill="#9bb0c5")
        pen.rectangle((18, 79, 737, 553), outline="#35495d")
        scene = model["frames"][index]["instances"]
        for material in scene:
            kind = int(material[0])
            x0 = 375 + (material[1] - middle[0]) * sc
            x1 = 375 + (material[4] - middle[0]) * sc
            y0 = 313 - (material[5] - middle[1]) * sc
            y1 = 313 - (material[2] - middle[1]) * sc
            if x1 - x0 < 2:
                x1 = x0 + 2
            if y1 - y0 < 2:
                y1 = y0 + 2
            pen.rectangle((x0, y0, x1, y1), outline=PALETTE[kind], width=1)
        for kind, name in enumerate(MATERIALS):
            pen.rectangle((24 + kind * 137, 562, 34 + kind * 137, 572),
                          fill=PALETTE[kind])
            pen.text((39 + kind * 137, 560), name.replace("_", " "),
                     fill="#c6d8e9")
        pen.text((765, 88), "MEASURED HOST LATENCY", fill="#cbdbe9")
        rows = sorted(model["runs"][index].values(),
                      key=lambda item: item["host_ms"])
        top = max(0.001, *(row["host_ms"] for row in rows))
        for j, row in enumerate(rows):
            y = 119 + j * 40
            color = "#a59cff" if row["gpu"] else "#5dd8c1"
            pen.text((765, y), row["mode"][:28], fill="#ccdaeb")
            pen.rectangle((765, y + 18,
                           765 + 190 * row["host_ms"] / top, y + 26),
                          fill=color)
            pen.text((960, y + 14), f"{row['host_ms']:.2f} ms",
                     fill="#dde8f5")
        pen.text((767, 510),
                 f"Exact candidate pairs: {model['runs'][index]['sap_rebuild']['pairs']:,}",
                 fill="#dce8f7")
        pen.text((767, 539), "Top-down envelopes; NOT a physics mesh movie",
                 fill="#a2b5c6")
        images.append(image)
    if not images:
        raise RuntimeError("No GIF frames")
    # Loop=0 means repeat indefinitely; disposal=2 prevents ghosted frames.
    images[0].save(
        output, format="GIF", save_all=True, append_images=images[1:],
        duration=125, loop=0, optimize=True, disposal=2
    )


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("experiment", type=Path)
    p.add_argument("--gpu", nargs="*", type=Path, default=None)
    p.add_argument("--html", type=Path)
    p.add_argument("--gif", type=Path)
    p.add_argument("--preview-only", action="store_true",
                   help="Show actual geometry during the benchmark without invented timings")
    args = p.parse_args()
    if args.preview_only and args.gif:
        raise ValueError("A timed GIF requires completed CPU evidence; use HTML for live geometry")
    directory = args.experiment
    gpu = args.gpu if args.gpu is not None else sorted(directory.glob("metal-*.csv"))
    dataset = load(directory, gpu, preview_only=args.preview_only)
    template = Path(__file__).resolve().parent.parent / "viewer" / "mass_stress.html"
    output = args.html or directory / "dashboard.html"
    if output.exists():
        raise FileExistsError(f"Preserving prior HTML report: {output}")
    payload = json.dumps(dataset, ensure_ascii=False,
                         separators=(",", ":"), allow_nan=False)
    payload = payload.replace("<", "\\u003c").replace("&", "\\u0026")
    markup = template.read_text(encoding="utf-8")
    if markup.count("__PAYLOAD__") != 1:
        raise ValueError("Incorrect static WebGL template placeholder")
    output.write_text(markup.replace("__PAYLOAD__", payload), encoding="utf-8")
    if args.gif:
        animated_gif(dataset, args.gif)
    binary_sha = sha256(directory / "snapshots.kmcb")
    print(
        f"PASS mass-material visual: {dataset['instances']} stepped worlds, "
        f"{dataset['primitives']} element AABBs, {len(dataset['frames'])} frames, "
        f"{dataset['gpu_checks']} GPU rows verified against CPU exact-pair hashes"
        + ("; GEOMETRY PREVIEW ONLY; NO TIMING CLAIM" if args.preview_only else "")
    )
    print("Immutable snapshot SHA-256: " + binary_sha)
    print("Interactive WebGL HTML: " + str(output))
    if args.gif:
        print("Looping GIF: " + str(args.gif))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
