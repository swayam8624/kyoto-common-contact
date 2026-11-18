#!/usr/bin/env python3
"""End-to-end, immutable, REAL glTF mesh CPU / Apple Metal research tournament.

One invocation:
- download 26 individually license-audited Khronos GLB assets if requested;
- import actual source triangle geometry and deterministic rigid trajectories;
- optionally open geometry-only WebGL while CPU/Metal measurement proceeds;
- run full-pair parity checked native CPU algorithms on the same Float32 AABBs;
- optionally run real Apple Metal GPU kernels + full pair-hash verification;
- extract exact AABB intersection markers, build offline real-solid-mesh viewer;
- record source/hardware/time/algorithm evidence without overwriting old runs.

This is a rigid motion broad-phase benchmark, NOT full material/contact physics.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time


def execute(argv: list[str], timeout: int) -> float:
    print("\n[REAL SCENE] " + " ".join(argv), flush=True)
    started = time.perf_counter()
    try:
        process = subprocess.run(argv, check=False, timeout=timeout)
    except subprocess.TimeoutExpired as e:
        raise RuntimeError("Safety time limit exceeded: " + " ".join(argv)) from e
    if process.returncode:
        raise RuntimeError(
            f"Exit {process.returncode} from: " + " ".join(argv)
        )
    seconds = time.perf_counter() - started
    print(f"[PASS] {seconds:.2f} wall seconds", flush=True)
    return seconds


def checksum(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            value.update(block)
    return value.hexdigest()


def summarize_cpu(path: Path) -> tuple[list[dict], list[dict]]:
    with path.open(newline="", encoding="utf-8") as file:
        data = list(csv.DictReader(file))
    frames = 1 + max(int(row["frame"]) for row in data)
    policies = set(row["mode"] for row in data)
    if "sap_rebuild" not in policies or "frozen_v4" not in policies:
        raise ValueError("No exact SAP/V4 control")
    if len(data) != frames * len(policies):
        raise ValueError("Incomplete CPU methods")
    groups: dict[str, list[dict]] = {mode: [] for mode in policies}
    references: dict[int, dict] = {}
    for row in data:
        if row["exact_sap_equal"] != "1":
            raise ValueError("Unverified CPU candidate parity")
        index = int(row["frame"])
        groups[row["mode"]].append(row)
        if row["mode"] == "sap_rebuild":
            references[index] = row
    if len(references) != frames:
        raise ValueError("Missing SAP frame")
    for mode, rows in groups.items():
        if len(rows) != frames or {int(x["frame"]) for x in rows} != set(range(frames)):
            raise ValueError(f"Missing/duplicated {mode} frame")
        for row in rows:
            ref = references[int(row["frame"])]
            if (row["pairs"], row["pair_hash"]) != (ref["pairs"], ref["pair_hash"]):
                raise ValueError(f"Full stable-pair hash mismatch on {mode}")
    warmup = min(8, frames // 8)
    summaries = []
    baseline = statistics.fmean(
        float(row["ms"]) for row in groups["frozen_v4"][warmup:]
    )
    for mode, rows in groups.items():
        sampled = sorted(float(row["ms"]) for row in rows[warmup:])
        mean = statistics.fmean(sampled)
        summaries.append({
            "method": mode, "mean_ms": mean,
            "p95_ms": sampled[max(0, math.ceil(.95 * len(sampled)) - 1)],
            "relative_to_v4": baseline / mean if mean > 0 else None,
        })
    summaries.sort(key=lambda x: x["mean_ms"])
    candidates = [
        {
            "frame": f, "total": int(references[f]["pairs"]),
            "inter_asset": int(references[f]["inter_instance_pairs"])
        } for f in range(frames)
    ]
    return summaries, candidates


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--assets", type=Path,
                   default=Path("output/cbp-khronos-assets-pinned"))
    p.add_argument("--download-assets", action="store_true",
                   help="Fetch pinned license-audited Khronos 26 GLBs if absent")
    p.add_argument("--output", type=Path, required=True,
                   help="Brand-new output case directory; never overwritten")
    p.add_argument("--build", type=Path, default=Path("build/kyoto-final-cpu"))
    p.add_argument("--frames", type=int, default=60)
    p.add_argument("--max-triangles-per-asset", type=int, default=512)
    p.add_argument("--max-visual-triangles-per-asset", type=int, default=2048,
                   help="Real GLB triangles rendered, NOT added to the benchmark")
    p.add_argument("--layout", choices=["wave", "crowded"], default="wave")
    p.add_argument("--seed", type=int, default=66217)
    p.add_argument("--gpu", action="store_true",
                   help="Run real Metal GPU SAP/fixed-region/online kernels; macOS only")
    p.add_argument("--open-preview", action="store_true",
                   help="On macOS, open actual triangles before benchmarking")
    p.add_argument("--open-report", action="store_true",
                   help="On macOS, open the completed self-contained 3D report")
    p.add_argument("--timeout", type=int, default=1500)
    args = p.parse_args()
    if (not 12 <= args.frames <= 240 or
        not 1 <= args.max_triangles_per_asset <= 2000 or
        not 1 <= args.max_visual_triangles_per_asset <= 6000):
        raise ValueError("Frame or triangle count beyond safe research cap")
    if not 30 <= args.timeout <= 3600:
        raise ValueError("Per-stage timeout must be 30..3600 seconds")
    if args.gpu and sys.platform != "darwin":
        raise RuntimeError("Actual Apple Metal benchmark needs macOS")
    if args.output.exists():
        raise FileExistsError("Refusing to overwrite benchmark evidence: " + str(args.output))
    here = Path(__file__).resolve().parent
    executables = {
        "cpu": args.build / "ccp_cbp_mass_replay",
        "hotspots": args.build / "ccp_cbp_contact_hotspots",
    }
    if args.gpu:
        executables["metal"] = args.build / "ccp_scaling_metal"
    for name, path in executables.items():
        if not path.is_file() or not os.access(path, os.X_OK):
            raise FileNotFoundError(f"Build executable {name}: {path}")
    if args.download_assets and not args.assets.exists():
        execute([
            sys.executable, str(here / "download_cbp_gltf_scenes.py"),
            "--output", str(args.assets),
        ], args.timeout)
    if not (args.assets / "ASSET_CATALOG.json").exists():
        raise FileNotFoundError(
            "Missing original source licenses and GLB checksums in "
            + str(args.assets) +
            ". Pass --download-assets or specify --assets."
        )

    stage = {}
    stage["import_seconds"] = execute([
        sys.executable, str(here / "import_cbp_gltf_scenes.py"),
        "--catalog", str(args.assets),
        "--output", str(args.output),
        "--frames", str(args.frames),
        "--max-triangles-per-asset", str(args.max_triangles_per_asset),
        "--max-visual-triangles-per-asset",
        str(args.max_visual_triangles_per_asset),
        "--layout", args.layout,
        "--seed", str(args.seed)
    ], args.timeout)

    if args.open_preview:
        preview = args.output / "REAL-3D-PREVIEW.html"
        stage["preview_seconds"] = execute([
            sys.executable, str(here / "render_cbp_gltf_scenes.py"),
            str(args.output), "--preview-only", "--html", str(preview),
        ], args.timeout)
        if sys.platform == "darwin":
            subprocess.run(["open", str(preview)], check=False)

    cpu = args.output / "cpu-core.csv"
    stage["cpu_seconds"] = execute([
        str(executables["cpu"]), str(args.output / "snapshots.kmcb"),
        str(args.output / "instances.csv"), str(cpu), "core",
    ], args.timeout)
    summaries, candidates = summarize_cpu(cpu)

    metal_stats = []
    if args.gpu:
        for mode in ("sap", "region", "online"):
            result = args.output / ("metal-" + mode + ".csv")
            stage[f"metal_{mode}_seconds"] = execute([
                str(executables["metal"]), "--replay",
                str(args.output / "snapshots.kmcb"),
                str(result), mode
            ], args.timeout)
            metal_stats.append({"backend": mode, "file": result.name})

    stage["spatial_seconds"] = execute([
        str(executables["hotspots"]),
        str(args.output / "snapshots.kmcb"),
        str(args.output / "instances.csv"),
        str(args.output / "contact-hotspots.jsonl"), "160",
    ], args.timeout)

    dashboard = args.output / "REAL-3D-OBSERVATORY.html"
    stage["viewer_seconds"] = execute([
        sys.executable, str(here / "render_cbp_gltf_scenes.py"),
        str(args.output), "--html", str(dashboard),
    ], args.timeout)

    models = json.loads((args.output / "asset-meshes.json").read_text())["objects"]
    manifest = {
        "schema": "CCP_REAL_GLTF_EXPERIMENT_V1",
        "completed_utc": datetime.now(timezone.utc).isoformat(),
        "hardware": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cpu_count": os.cpu_count(),
            "python": sys.version.split()[0],
        },
        "scope": ("Real source triangle geometry, sampled and moved rigidly; "
                  "no inter-object contact response, CCD, glTF skeletal "
                  "animation, full original mesh or GPU port of the CPU portfolio."),
        "models_imported": len(models),
        "source_triangles": sum(x["source_triangles"] for x in models),
        "triangle_aabbs_recorded": sum(x["selected_triangles"] for x in models),
        "visual_glb_triangles_rendered": sum(
            x.get("display_triangles",x["selected_triangles"]) for x in models
        ),
        "frames": args.frames,
        "layout": args.layout,
        "seed": args.seed,
        "gpu_measured": args.gpu,
        "cpu_methods": summaries,
        "gpu_modes": metal_stats,
        "cross_asset_first_frame": next(
            (x["frame"] for x in candidates if x["inter_asset"] > 0), None
        ),
        "cross_asset_last_frame": candidates[-1]["inter_asset"],
        "candidate_pairs_last_frame": candidates[-1]["total"],
        "snapshots_sha256": checksum(args.output / "snapshots.kmcb"),
        "duration_seconds": stage,
        "downloaded_originals": str(args.assets / "ASSET_CATALOG.json"),
    }
    (args.output / "REAL-SCENE-EXPERIMENT.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print("\n[REAL GLTF EVIDENCE COMPLETE]")
    print(f"{len(models)} actual Khronos GLB models, "
          f"{manifest['triangle_aabbs_recorded']:,} measured triangle AABBs, "
          f"{manifest['visual_glb_triangles_rendered']:,} true source triangles displayed "
          f"(distinct visualization LOD), from {manifest['source_triangles']:,} "
          f"original model triangles")
    print(f"Exact {args.frames}-frame CPU candidate-parity verification; "
          f"Metal actually executed: {'YES' if args.gpu else 'NO'}")
    for item in summaries:
        print(f"  {item['method']:<26} mean {item['mean_ms']:.5f} ms "
              f"P95 {item['p95_ms']:.5f} ms, V4/method "
              f"{item['relative_to_v4']:.3f}x")
    print("3D real mesh visualization: " + str(dashboard))
    print("Inspectable metadata: " + str(args.output / "REAL-SCENE-EXPERIMENT.json"))
    if args.open_report and sys.platform == "darwin":
        subprocess.run(["open", str(dashboard)], check=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
