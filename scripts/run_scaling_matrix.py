#!/usr/bin/env python3
"""Reproducible Kyoto CPU/GPU benchmark orchestrator.

Runs one executable at a time (never competing CPU and GPU jobs), preserves
individual raw CSVs, records a hardware/compiler manifest, and refuses missing,
incompatible or incorrectly labelled outputs. Does not fabricate GPU results.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import subprocess
import sys
from datetime import datetime, timezone

METHODS = ("cpu", "regional-fixed", "regional-adaptive", "regional-hier",
           "metal-sap", "metal-exhaustive")
SCENES = ("sparse", "cluster", "dense", "hotspot", "all_overlap")
METHOD_LABELS = {
    "cpu": {"cpu_sap_serial", "cpu_sap_threaded", "cpu_sap_work_aware", "cpu_sap_auto"},
    "regional-fixed": {"cpu_region_fixed"},
    "regional-adaptive": {"cpu_region_adaptive"},
    "regional-hier": {"cpu_region_hierarchical"},
    "metal-sap": {"metal_sap_sorted"},
    "metal-exhaustive": {"metal_exhaustive_reference"},
}


def execute(cmd, cwd=None):
    result = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True,
                            timeout=900, check=False)
    return {"command": cmd, "returncode": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def executable_for(method, build):
    executable = "ccp_scaling_cpu" if method == "cpu" else (
        "ccp_scaling_metal" if method.startswith("metal") else
        "ccp_scaling_regional")
    path = build / executable
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f"Missing executable: {path}; build this target first")
    return path


def arguments(method, path, n, frames, threads, scene, seed, cell_width):
    if method == "cpu":
        return [str(path), str(n), str(frames), str(threads), scene, str(seed)]
    if method.startswith("regional"):
        mode = method.split("-", 1)[1]
        return [str(path), str(n), str(frames), scene, str(seed), str(cell_width),
                "hierarchical" if mode == "hier" else mode]
    return [str(path), str(n), str(frames), scene, str(seed),
            "sap" if method == "metal-sap" else "exhaustive"]


def validate_csv(path, method, frames, n, threads):
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected_labels = METHOD_LABELS[method]
    if method == "cpu" and threads == 1:
        expected_labels = {"cpu_sap_serial", "cpu_sap_auto"}
    if not rows:
        raise RuntimeError(f"Empty benchmark CSV: {path}")
    if {r["backend"] for r in rows} != expected_labels:
        raise RuntimeError(f"Wrong backend labels in {path}")
    if len(rows) != frames * len(expected_labels):
        raise RuntimeError(f"Incomplete CSV frames in {path}")
    for row in rows:
        if int(row["n"]) != n or int(row["frame"]) not in range(frames):
            raise RuntimeError(f"Incorrect scene metadata in {path}")
        if int(row["misses"]) != 0:
            raise RuntimeError(f"Incorrect collision coverage in {path}")
        if float(row["host_ms"] if method.startswith("metal") else row["ms"]) < 0:
            raise RuntimeError(f"Negative wall time in {path}")
    for label in expected_labels:
        observed = [int(r["frame"]) for r in rows if r["backend"] == label]
        if sorted(observed) != list(range(frames)):
            raise RuntimeError(f"Missing or duplicate frames in {path}, {label}")
    return rows


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--methods", nargs="+", choices=METHODS,
                        default=["cpu", "regional-fixed", "regional-adaptive",
                                 "regional-hier"])
    parser.add_argument("--scenes", nargs="+", choices=SCENES, default=["hotspot"])
    parser.add_argument("--sizes", nargs="+", type=int, default=[512, 2048])
    parser.add_argument("--frames", type=int, default=20)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--threads", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--seed", type=int, default=1234)
    parser.add_argument("--cell-width", type=float, default=0.6)
    args = parser.parse_args()
    if args.frames < 1 or args.frames > 10000 or args.repeats < 1 or args.repeats > 100:
        parser.error("frames or repeats outside supported bounds")
    if args.threads < 1 or args.threads > 256:
        parser.error("threads outside supported bounds")
    if any(size < 1 or size > 50000 for size in args.sizes):
        parser.error("size outside safe research range")
    if any(size > 2048 for size in args.sizes) and "all_overlap" in args.scenes:
        parser.error("all_overlap output grows quadratically; cap at 2048")
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("output directory must be new/empty; never overwrite raw evidence")
    args.output.mkdir(parents=True, exist_ok=True)
    build = args.build.resolve()
    repo = Path(__file__).resolve().parents[1]
    manifest = {"created_utc": datetime.now(timezone.utc).isoformat(),
                "platform": platform.platform(), "python": sys.version,
                "cpu_count": os.cpu_count(), "machine": platform.machine(),
                "cpu_info": platform.processor(), "parameters": vars(args).copy(),
                "git": execute(["git", "rev-parse", "HEAD"], repo)["stdout"].strip(),
                "compiler": execute(["c++", "--version"])["stdout"].splitlines()[:2],
                "runs": []}
    manifest["parameters"]["build"] = str(args.build)
    manifest["parameters"]["output"] = str(args.output)
    variants = [(method, scene, n, repeat)
                for method in args.methods for scene in args.scenes
                for n in args.sizes for repeat in range(args.repeats)]
    random.Random(args.seed).shuffle(variants)
    failed = False
    for method, scene, n, repeat in variants:
        name = f"{method}_{scene}_n{n}_rep{repeat:03d}.csv"
        file = args.output / name
        exe = executable_for(method, build)
        # Keep identical random scene seeds across methods for each repeat.
        seed = args.seed + repeat * 10007
        cmd = [str(exe), str(file)] + arguments(
            method, file, n, args.frames, args.threads,
            scene, seed, args.cell_width)[1:]
        result = execute(cmd, repo)
        entry = {"method": method, "scene": scene, "n": n, "repeat": repeat,
                 "seed": seed, "returncode": result["returncode"],
                 "command": cmd, "stdout": result["stdout"][-4000:],
                 "stderr": result["stderr"][-4000:]}
        if result["returncode"] == 0:
            try:
                rows = validate_csv(file, method, args.frames, n, args.threads)
                entry["rows"] = len(rows)
                entry["sha256"] = sha256(file)
            except Exception as error:
                entry["validation_error"] = str(error)
                failed = True
        else:
            failed = True
        manifest["runs"].append(entry)
        (args.output / "manifest.json").write_text(
            json.dumps(manifest, indent=2, default=str), encoding="utf-8")
        print(f"{'FAIL' if entry.get('validation_error') or entry['returncode'] else 'PASS'} "
              f"{method} {scene} n={n} repetition={repeat}", flush=True)
    if failed:
        raise SystemExit("One or more benchmark executions failed; inspect manifest")
    print(f"All {len(variants)} executions completed; raw evidence in {args.output}")


if __name__ == "__main__":
    main()
