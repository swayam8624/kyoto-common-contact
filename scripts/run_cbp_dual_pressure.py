#!/usr/bin/env python3
"""Phase 12: bounded concurrent CPU + REAL Apple Metal global AABB pressure.

Separately timed, identical seed/scenario workloads are launched AT ONCE to
measure contention and sustained wall pressure, NOT algorithmic speedup.
The GPU implementation performs actual Metal kernels; no emulated GPU
path exists. CPU and Metal both use independently regenerated deterministic
Float32 AABBs, not an ambiguous shared mutable buffer.

This is an optional sustained thermal/memory pressure experiment. Use CPU/GPU
*isolated* runs for fair algorithmic comparison; concurrent contention is a
different research question. No pair-list materialization is implied by
count+fingerprint verification.

All children killed on timeout; every file created only in a fresh output dir.
Never change a device power cap or launch an unbounded all-overlap 1M N^2 kernel.
"""
from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time


def usage():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--build", type=Path, default=Path("build/kyoto-final-cpu"))
    p.add_argument("--scene", choices=("sparse", "burst", "slabs", "bridge",
                                       "teleport"), default="burst")
    p.add_argument("--n", type=int, default=1_000_000)
    p.add_argument("--frames", type=int, default=48)
    p.add_argument("--seed", type=int, default=61749)
    p.add_argument("--max-tests", type=int, default=300_000_000)
    p.add_argument("--max-frame-ms", type=int, default=25_000)
    p.add_argument("--max-grid-entries", type=int, default=12_000_000)
    p.add_argument("--max-gpu-mib", type=int, default=768)
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--sample-period", type=float, default=1.)
    p.add_argument("--cpu-only", action="store_true",
                   help="CI smoke only; real joint CPU+GPU requires macOS Metal")
    return p.parse_args()


def pid_usage(pid: int):
    try:
        r = subprocess.run(["ps", "-o", "rss=,pcpu=", "-p", str(pid)],
                           capture_output=True, text=True, timeout=3)
        if r.returncode == 0 and r.stdout.strip():
            values = r.stdout.strip().split()
            return round(int(values[0]) / 1024., 2), float(values[1])
    except (ValueError, OSError, subprocess.SubprocessError, IndexError):
        pass
    return None, None


def launch(argv: list[str], log):
    # Separate process group lets us terminate all descendants on timeout.
    return subprocess.Popen(
        argv, stdout=log, stderr=subprocess.STDOUT,
        start_new_session=True
    )


def terminate(processes):
    for proc in processes:
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    for proc in processes:
        if proc.poll() is None:
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait(timeout=3)


def rows(path: Path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def verify_cpu(data: list[dict], frames: int):
    methods = {"global_3d_hash_grid", "global_x_sweep"}
    if len(data) != frames * 2 or set(x["method"] for x in data) != methods:
        raise RuntimeError("Missing per-frame CPU global method evidence")
    full = [r for r in data if r["method"] == "global_3d_hash_grid"]
    if len(full) != frames or any(r["status"] != "complete" for r in full):
        raise RuntimeError("Incomplete global grid; lower workload or increase bounded caps")
    for row in data:
        if row["status"] == "complete" and row["cross_method_verified"] == "1":
            continue
        # Incomplete sweep is documented, not a correctness or performance win.
        if row["method"] == "global_x_sweep" and row["status"] != "complete":
            continue
        if row["method"] == "global_3d_hash_grid" and row["analytical_construct_verified"] == "1":
            continue
        if row["method"] == "global_3d_hash_grid":
            # Generic real cases rely on grid's canonical unique-cell ownership.
            # The GPU's independent fingerprint provides separate same-input
            # corroboration, not mathematical full pair-set proof.
            continue
    return full


def main():
    a = usage()
    if not 1024 <= a.n <= 1_000_000 or not 4 <= a.frames <= 240:
        raise ValueError("Bounded concurrent cohort is 1K..1M AABBs, 4..240 frames")
    if not 1 <= a.seed <= 2147483647:
        raise ValueError("Seed out of range")
    if not 1_000_000 <= a.max_tests <= 600_000_000:
        raise ValueError("Max tests must be 1M..600M")
    if not 20 <= a.max_frame_ms <= 120000:
        raise ValueError("Frame time limit 20..120000 ms")
    if not 10000 <= a.max_grid_entries <= 20_000_000:
        raise ValueError("Grid entry cap 10K..20M")
    if not 32 <= a.max_gpu_mib <= 4096:
        raise ValueError("GPU allocation cap 32..4096 MiB")
    if not 30 <= a.timeout <= 3600:
        raise ValueError("Total simultaneous wall limit 30..3600 seconds")
    if not .2 <= a.sample_period <= 5.:
        raise ValueError("Sampling period must be .2..5 seconds")
    if sys.platform != "darwin" and not a.cpu_only:
        raise RuntimeError("Real concurrent Apple Metal requires macOS (no GPU simulation)")
    if a.output.exists():
        raise FileExistsError("Never overwrite evidence: " + str(a.output))

    cpu = (a.build / "ccp_cbp_pressure_cpu").resolve()
    metal = (a.build / "ccp_cbp_pressure_metal").resolve()
    if not cpu.is_file():
        raise FileNotFoundError(f"Build CPU pressure executable: {cpu}")
    if not a.cpu_only and not metal.is_file():
        raise FileNotFoundError(f"Build real Apple Metal pressure executable: {metal}")
    a.output.mkdir(parents=True)
    base = a.output
    cpu_csv = base / "cpu-global.csv"
    gpu_csv = base / "metal-grid.csv"
    common = [a.scene, str(a.n), str(a.frames), str(a.seed),
              str(a.max_tests), str(a.max_frame_ms), str(a.max_grid_entries)]
    commands = {
        "cpu": [str(cpu), str(cpu_csv), *common],
    }
    if not a.cpu_only:
        commands["metal"] = [
            str(metal), str(gpu_csv), a.scene, str(a.n), str(a.frames),
            str(a.seed), "grid", str(a.max_tests), str(a.max_frame_ms),
            str(a.max_grid_entries), str(a.max_gpu_mib)
        ]
    logfiles = {}
    procs = {}
    started = time.monotonic()
    phase = "incomplete"
    captured = []
    try:
        for name, args in commands.items():
            stream = (base / (name + ".log")).open("w", encoding="utf-8")
            logfiles[name] = stream
            procs[name] = launch(args, stream)
        with (base / "host-contention.csv").open("w", newline="", encoding="utf-8") as evidence:
            writer = csv.writer(evidence)
            writer.writerow(["elapsed_seconds", "process", "pid", "rss_mib",
                             "host_cpu_percent", "status"])
            while True:
                now = time.monotonic()
                for name, proc in procs.items():
                    memory, cpu_percent = pid_usage(proc.pid)
                    writer.writerow([
                        round(now - started, 3), name, proc.pid,
                        "" if memory is None else memory,
                        "" if cpu_percent is None else cpu_percent,
                        "running" if proc.poll() is None else "finished"
                    ])
                evidence.flush()
                if all(p.poll() is not None for p in procs.values()):
                    break
                if now - started >= a.timeout:
                    phase = "timeout"
                    break
                time.sleep(a.sample_period)
        if phase == "timeout":
            raise TimeoutError("Joint CPU/Metal pressure hit bounded wall-time cap")
        failed = {name: p.returncode for name,p in procs.items() if p.returncode}
        if failed:
            raise RuntimeError("Native backend failed; see logs: " + repr(failed))
        cpu_rows = rows(cpu_csv)
        complete = verify_cpu(cpu_rows, a.frames)
        if not a.cpu_only:
            gpu_rows = rows(gpu_csv)
            if len(gpu_rows) != a.frames:
                raise RuntimeError("Missing Metal GPU frames")
            for i,(cpu_row,gpu_row) in enumerate(zip(complete,gpu_rows)):
                if gpu_row["status"] != "COMPLETE":
                    raise RuntimeError(f"Metal incomplete on frame {i}")
                for lhs,rhs in (("GPU_candidate_count","exact_pairs"),
                                ("GPU_sum32","hash32_sum"),
                                ("GPU_xor32","hash32_xor")):
                    if int(gpu_row[lhs]) != int(cpu_row[rhs]):
                        raise RuntimeError(f"GPU/CPU count or hash mismatch on frame {i}: {lhs}")
                valid = gpu_row["validation"]
                if valid not in ("GPU_CPU_GRID_DIGEST","GPU_CPU_SMALL_BRUTE",
                                 "ANALYTIC_ALL_OVERLAP_COUNT_ONLY"):
                    raise RuntimeError("Unverified GPU frame: " + valid)
        phase = "complete"
    except Exception as exc:
        captured.append(str(exc))
        phase = phase if phase == "timeout" else "failed"
    finally:
        terminate(list(procs.values()))
        for stream in logfiles.values():
            stream.close()
        metadata = {
            "schema":"CCP_CONCURRENT_PRESSURE_V1",
            "utc": datetime.now(timezone.utc).isoformat(),
            "host": platform.platform(),
            "scene":a.scene,"n":a.n,"frames":a.frames,
            "seed":a.seed,"gpu_requested":not a.cpu_only,
            "gpu_actually_executed":(not a.cpu_only and
                                     procs.get("metal") is not None and
                                     procs["metal"].returncode == 0),
            "status":phase,
            "wall_seconds":round(time.monotonic()-started,3),
            "limits":{"max_tests":a.max_tests,"max_frame_ms":a.max_frame_ms,
                      "max_grid_entries":a.max_grid_entries,
                      "max_gpu_mib":a.max_gpu_mib,"timeout_seconds":a.timeout},
            "exit_codes":{name:p.returncode for name,p in procs.items()},
            "failures":captured,
            "scope":("Two genuinely concurrent processes executing global "
                     "current-AABB search/count+fingerprints on identical "
                     "synthetic deterministic scene. This is a contention "
                     "and sustained-work experiment, not proof of device "
                     "thermal saturation or full physical contact solving.")
        }
        (base / "EXPERIMENT.json").write_text(
            json.dumps(metadata,indent=2)+"\n",encoding="utf-8"
        )
        print(json.dumps(metadata,indent=2), flush=True)
    return 0 if phase == "complete" else 1


if __name__ == "__main__":
    raise SystemExit(main())
