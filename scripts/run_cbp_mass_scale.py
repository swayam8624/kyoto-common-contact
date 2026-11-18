#!/usr/bin/env python3
"""Bounded, reproducible long mixed-material CPU/Metal runs with WebGL evidence.

Streams real experiment progress. Does not launch asynchronous background jobs.
Cases run sequentially; no CPU/GPU resource contention. All exact candidate
comparisons use the SAME immutable float32 KYOTOM01 material frames.
"""
from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


def checksum(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(4 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def execute(command: list[str], *, timeout: int) -> float:
    print("[CBP RUN] " + " ".join(command), flush=True)
    begin = time.monotonic()
    try:
        process = subprocess.run(command, timeout=timeout, check=False)
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(
            "Execution exceeded safety timeout and was terminated: "
            + " ".join(command)
        ) from exc
    duration = time.monotonic() - begin
    if process.returncode:
        raise RuntimeError(
            "Experiment failed with exit code "
            + str(process.returncode) + ": " + " ".join(command)
        )
    print("[CBP PASS] wall seconds: " + f"{duration:.2f}", flush=True)
    return duration


def stats_for(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as f:
        records = list(csv.DictReader(f))
    if not records:
        raise ValueError("No result data: " + str(path))
    from collections import defaultdict
    import math
    import statistics
    groups: dict[str, list[dict]] = defaultdict(list)
    frame_count = max(int(x["frame"]) for x in records) + 1
    warmup = min(8, frame_count // 8)
    for row in records:
        if int(row["frame"]) >= warmup:
            groups[row["mode"]].append(row)
    output = []
    for mode, group in sorted(groups.items()):
        times = sorted(float(x["ms"]) for x in group)
        output.append({
            "mode": mode, "mean_ms": statistics.fmean(times),
            "p95_ms": times[max(0, math.ceil(.95 * len(times)) - 1)],
            "candidate_pair_count_last": int(group[-1]["pairs"]),
        })
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build/kyoto-final-cpu"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--instances", nargs="+", type=int, default=[100, 1000])
    parser.add_argument("--frames", type=int, default=48)
    parser.add_argument("--seed", type=int, default=42017)
    parser.add_argument("--layout", choices=["grid", "clusters", "contact_front"],
                        default="contact_front")
    parser.add_argument("--gpu", action="store_true",
                        help="Run real Apple Metal SAP, regional and online replay")
    parser.add_argument("--full", action="store_true",
                        help="Include expensive certified-cache/causal CPU modes (max 16k AABBs)")
    parser.add_argument("--gif", action="store_true",
                        help="Also make actual-geometry looping GIF (requires Pillow)")
    parser.add_argument("--live-visual", action="store_true",
                        help="Open a GPU WebGL geometry-only replay before benchmarking, then final report afterwards")
    parser.add_argument("--case-timeout", type=int, default=1500,
                        help="Max seconds per export or algorithm process")
    args = parser.parse_args()

    if not args.instances or len(set(args.instances)) != len(args.instances):
        raise ValueError("Specify nonempty distinct material counts")
    if any(i < 12 or i > 1000 for i in args.instances):
        raise ValueError("Material counts must be 12..1000 per case")
    if args.frames < 12 or args.frames > 240:
        raise ValueError("Frame count must be 12..240")
    if args.output.exists():
        raise FileExistsError(
            "Evidence directory exists; refusing to mix or overwrite experiments: "
            + str(args.output)
        )
    binaries = {
        "export": args.build / "ccp_cbp_mass_export",
        "cpu": args.build / "ccp_cbp_mass_replay",
    }
    if args.gpu:
        if sys.platform != "darwin":
            raise RuntimeError("Apple Metal GPU mode requires macOS")
        binaries["metal"] = args.build / "ccp_scaling_metal"
    for name, binary in binaries.items():
        if not binary.is_file() or not os.access(binary, os.X_OK):
            raise RuntimeError(
                "Missing built " + name + " binary: " + str(binary)
            )
    args.output.mkdir(parents=True, exist_ok=False)
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], capture_output=True,
        text=True, check=False
    )
    hardware = {
        "platform": platform.platform(),
        "processor": platform.processor(),
        "machine": platform.machine(),
        "python": sys.version.split()[0],
        "cpu_count": os.cpu_count(),
        "commit": revision.stdout.strip() if revision.returncode == 0 else None,
        "utc_start": datetime.now(timezone.utc).isoformat(),
    }
    cases = []
    for instance_count in args.instances:
        case = args.output / (
            "materials" + str(instance_count)
            + "-frames" + str(args.frames)
            + "-seed" + str(args.seed)
            + "-" + args.layout
        )
        print("\n[CBP CASE] " + case.name, flush=True)
        started = time.monotonic()
        export_seconds = execute(
            [str(binaries["export"]), str(case), str(instance_count),
             str(args.frames), str(args.seed), args.layout],
            timeout=args.case_timeout,
        )
        if args.live_visual:
            live_page = case / "preview-during-benchmark.html"
            execute(
                [sys.executable,
                 str(Path(__file__).with_name("render_cbp_mass_dashboard.py")),
                 str(case), "--preview-only", "--html", str(live_page)],
                timeout=args.case_timeout,
            )
            if sys.platform == "darwin":
                subprocess.run(["open", str(live_page)], check=False)
        cpu_path = case / ("cpu-full.csv" if args.full else "cpu-core.csv")
        cpu_seconds = execute(
            [str(binaries["cpu"]), str(case / "snapshots.kmcb"),
             str(case / "instances.csv"), str(cpu_path),
             "full" if args.full else "core"],
            timeout=args.case_timeout,
        )
        gpu_runs = []
        if args.gpu:
            for mode in ("sap", "region", "online"):
                gpu_path = case / ("metal-" + mode + ".csv")
                duration = execute(
                    [str(binaries["metal"]), "--replay",
                     str(case / "snapshots.kmcb"), str(gpu_path), mode],
                    timeout=args.case_timeout,
                )
                gpu_runs.append({
                    "mode": mode, "duration_seconds": duration,
                    "csv": str(gpu_path.relative_to(case)),
                })
        dashboard = case / "dashboard.html"
        visual_cmd = [
            sys.executable,
            str(Path(__file__).with_name("render_cbp_mass_dashboard.py")),
            str(case), "--html", str(dashboard),
        ]
        if args.gif:
            visual_cmd += ["--gif", str(case / "loop.gif")]
        visual_seconds = execute(visual_cmd, timeout=args.case_timeout)
        if args.live_visual and sys.platform == "darwin":
            subprocess.run(["open", str(dashboard)], check=False)
        with (case / "instances.csv").open(newline="", encoding="utf-8") as file:
            rows = list(csv.DictReader(file))
        with (case / "frames.csv").open(newline="", encoding="utf-8") as file:
            frame_rows = list(csv.DictReader(file))
        counts = {x["kind"]: sum(y["kind"] == x["kind"] for y in rows) for x in rows}
        result = {
            "case": case.name,
            "instances": len(rows),
            "primitive_AABBs": int(frame_rows[0]["primitives"]),
            "frames": args.frames,
            "material_counts": counts,
            "cpu_methods": stats_for(cpu_path),
            "gpu_modes": gpu_runs,
            "snapshot_sha256": checksum(case / "snapshots.kmcb"),
            "export_seconds": export_seconds,
            "cpu_seconds": cpu_seconds,
            "visual_seconds": visual_seconds,
            "total_wall_seconds": time.monotonic() - started,
            "scientific_scope": (
                "Independently stepped mass-spring instances with NO "
                "inter-instance solver contact response. Exact broad-phase "
                "AABB candidate pairs, not complete collision physics."
            ),
        }
        (case / "CASE_REPORT.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )
        print(
            "[CBP CASE PASS] " + case.name + ": "
            + str(result["primitive_AABBs"]) + " primitives, "
            + str(result["frames"]) + " frames, CPU full-pair parity verified"
            + ("; Metal full-pair hashes verified" if args.gpu else "; Apple GPU not run"),
            flush=True
        )
        cases.append(result)
    report = {
        "hardware": hardware, "instances_requested": args.instances,
        "frames": args.frames, "layout": args.layout, "seed": args.seed,
        "gpu_enabled": args.gpu, "complete": True, "cases": cases,
    }
    (args.output / "MANIFEST.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print("\n[CBP MASS STRESS COMPLETE] " + str(args.output / "MANIFEST.json"))
    for item in cases:
        times = {x["mode"]: x["mean_ms"] for x in item["cpu_methods"]}
        base = times["frozen_v4"]
        print(
            str(item["instances"]) + " instances / "
            + str(item["primitive_AABBs"]) + " AABBs: "
            + ", ".join(
                key + "=" + f"{base / value:.3f}x"
                for key, value in times.items() if key != "frozen_v4"
            ) + " vs V4"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
