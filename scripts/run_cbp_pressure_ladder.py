#!/usr/bin/env python3
"""Phase 11 HARDWARE PRESSURE LADDER — true global CPU/Apple Metal AABB work.

No disjoint-island shortcuts. All algorithmic work (including generated
candidate pairs that we only COUNT) is exact under declared work caps.
Every truncated or unverified run is explicitly marked, not averaged.

Example on Apple Silicon:
  python3 scripts/run_cbp_pressure_ladder.py --tier serious --gpu \
    --output output/phase11-serious-mac
This command can exercise a CPU/GPU for several minutes. It is synchronous
and enforces per-process and whole-suite wall-clock limits. Workload sizes
are not an assertion that machine thermal or memory peaks were reached.
"""
from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone
import html
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import time


def scenarios(tier: str) -> list[tuple[str, int, int]]:
    if tier == "calibration":
        return [
            ("sparse", 1024, 8), ("burst", 4096, 12),
            ("slabs", 8192, 4), ("all_overlap", 2048, 3)
        ]
    if tier == "serious":
        return [
            ("sparse", 65536, 12), ("burst", 65536, 24),
            ("slabs", 65536, 8), ("bridge", 131072, 16),
            ("teleport", 250000, 16), ("burst", 1000000, 20),
            ("slabs", 1000000, 6), ("all_overlap", 8192, 6)
        ]
    if tier == "extreme":
        return [
            ("sparse", 1000000, 30), ("burst", 1000000, 120),
            ("slabs", 1000000, 30), ("bridge", 1000000, 24),
            ("teleport", 1000000, 30), ("burst", 2000000, 20),
            ("all_overlap", 16384, 10)
        ]
    raise ValueError("Unknown tier")


def probe_memory_gib():
    if sys.platform == "darwin":
        try:
            value = subprocess.check_output(
                ["sysctl", "-n", "hw.memsize"], text=True, timeout=5
            )
            return int(value.strip()) / 2**30
        except (OSError, ValueError, subprocess.SubprocessError):
            return None
    try:
        return int(os.sysconf("SC_PHYS_PAGES") *
                   os.sysconf("SC_PAGE_SIZE")) / 2**30
    except (ValueError, OSError, AttributeError):
        return None


def process(argv: list[str], logfile: Path, timeout: int) -> dict:
    started = time.monotonic()
    print("\n[PRESSURE] " + " ".join(argv), flush=True)
    try:
        with logfile.open("w", encoding="utf-8") as f:
            result = subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
        outcome = "completed" if result.returncode == 0 else "failed"
        status = result.returncode
    except subprocess.TimeoutExpired:
        outcome, status = "TIMEOUT", -1
    seconds = time.monotonic() - started
    print("[PRESSURE] " + outcome + f" wall_seconds={seconds:.2f} "
          + str(logfile), flush=True)
    return {"argv": argv, "outcome": outcome, "exit_status": status,
            "wall_seconds": round(seconds, 3), "log": str(logfile)}


def read(path: Path):
    if not path.is_file():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def summarize(rows, complete="complete", field="query_ms"):
    ok = [r for r in rows if r["status"] == complete]
    if not ok:
        return {"complete_frames": 0, "total_frames": len(rows)}
    ms = sorted(float(r[field]) for r in ok)
    return {
        "complete_frames": len(ok), "total_frames": len(rows),
        "mean_ms": round(statistics.fmean(ms), 5),
        "p95_ms": round(ms[math.ceil(.95 * len(ms)) - 1], 5),
        "max_ms": round(max(ms), 5),
        "last_complete_count": int(ok[-1].get(
            "exact_pairs", ok[-1].get("GPU_candidate_count",
            ok[-1].get("GPU_candidates", "0")))),
        "max_tested": max(int(r.get(
            "tested_pairs", r.get("GPU_tested_pairs", r.get("GPU_tests", "0")))) for r in ok),
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--tier", choices=("calibration", "serious", "extreme"),
                   default="calibration")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--build", type=Path, default=Path("build/kyoto-final-cpu"))
    p.add_argument("--seed", type=int, default=61749)
    p.add_argument("--gpu", action="store_true",
                   help="Execute REAL native Apple Metal grid and all-pairs counts")
    p.add_argument("--vulkan", action="store_true",
                   help="Also run actual Vulkan SPIR-V compute if built; software "
                        "Vulkan is explicitly labeled SOFTWARE_CPU")
    p.add_argument("--max-tests", type=int, default=300_000_000)
    p.add_argument("--max-frame-ms", type=int, default=25_000)
    p.add_argument("--max-grid-entries", type=int, default=12_000_000)
    p.add_argument("--max-gpu-mib", type=int, default=768)
    p.add_argument("--stage-timeout", type=int, default=900)
    p.add_argument("--suite-timeout", type=int, default=3600)
    p.add_argument("--force-large-memory", action="store_true",
                   help="Opt in to largest workloads on machines under 16 GiB")
    a = p.parse_args()
    if not 1 <= a.seed <= 2147483647:
        raise ValueError("Seed outside C++ generator range")
    if not 1000 <= a.max_tests <= 600_000_000:
        raise ValueError("Test budget is 1K to 600M.")
    if not 20 <= a.max_frame_ms <= 120_000:
        raise ValueError("Per-method frame budget 20ms to 120s")
    if not 10000 <= a.max_grid_entries <= 20_000_000:
        raise ValueError("Grid entry cap 10K to 20M")
    if not 32 <= a.max_gpu_mib <= 4096:
        raise ValueError("GPU memory cap 32-4096 MiB")
    if not 30 <= a.stage_timeout <= 3600 or not 120 <= a.suite_timeout <= 21600:
        raise ValueError("Timeouts outside bounded safety range")
    if a.gpu and sys.platform != "darwin":
        raise RuntimeError("Real Apple Metal requested on non-macOS host")
    if a.output.exists():
        raise FileExistsError("Never overwrite experiment evidence: " + str(a.output))

    cpu = (a.build / "ccp_cbp_pressure_cpu").resolve()
    metal = (a.build / "ccp_cbp_pressure_metal").resolve()
    vulkan = (a.build / "ccp_cbp_pressure_vulkan").resolve()
    if not cpu.is_file():
        raise FileNotFoundError("Build CPU pressure executable: " + str(cpu))
    if a.gpu and not metal.is_file():
        raise FileNotFoundError("Build real Metal pressure executable: " + str(metal))
    if a.vulkan and not vulkan.is_file():
        raise FileNotFoundError("Install Vulkan SDK+glslangValidator and build "
                                "ccp_cbp_pressure_vulkan: " + str(vulkan))
    memory = probe_memory_gib()
    if (a.tier == "extreme" and memory is not None and memory < 16
            and not a.force_large_memory):
        raise RuntimeError(
            "Extreme 2M-body cohort on <16 GiB system requires "
            "explicit --force-large-memory; use --tier serious first"
        )
    a.output.mkdir(parents=True)
    (a.output / "logs").mkdir()
    started = time.monotonic()
    receipt = {
        "schema": "CCP_PRESSURE_LADDER_V1",
        "time_utc": datetime.now(timezone.utc).isoformat(),
        "host": platform.platform(), "processor": platform.processor(),
        "machine": platform.machine(), "ram_gib": memory,
        "metal_requested": a.gpu, "vulkan_requested": a.vulkan,
        "tier": a.tier, "seed": a.seed,
        "algorithmic_scope": (
            "GENERAL global exact AABB count+fingerprint search. CPU grid and "
            "sweep and native Metal grid/exhaustive. No island-separation "
            "assumptions, no pair list, no continuous collision detection, "
            "not a fully coupled deformable-material benchmark."),
        "caps": {
            "max_tests": a.max_tests, "max_frame_ms": a.max_frame_ms,
            "max_grid_entries": a.max_grid_entries,
            "max_gpu_mib": a.max_gpu_mib, "stage_timeout": a.stage_timeout,
            "suite_timeout": a.suite_timeout,
        },
        "runs": [],
        "summaries": [],
    }
    try:
        for scene, n, frames in scenarios(a.tier):
            if time.monotonic() - started > a.suite_timeout:
                receipt["stopped_reason"] = "Whole-suite wall-time budget"
                break
            stem = f"{scene}-n{n}-f{frames}"
            source = a.output / (stem + "-cpu.csv")
            report = process([
                str(cpu), str(source), scene, str(n), str(frames), str(a.seed),
                str(a.max_tests), str(a.max_frame_ms),
                str(a.max_grid_entries),
            ], a.output / "logs" / (stem + "-cpu.log"),
                min(a.stage_timeout, max(30, int(a.suite_timeout - (
                    time.monotonic() - started)))))
            receipt["runs"].append(report)
            if report["outcome"] != "completed":
                # Do not silently turn a failed geometry/budget case into a
                # successful summary, even if other stages are healthy.
                receipt["summaries"].append({
                    "scene": scene, "n": n, "method": "cpu_global",
                    "status": report["outcome"]
                })
                continue
            rows = read(source)
            for mode in ("global_3d_hash_grid", "global_x_sweep"):
                subset = [r for r in rows if r["method"] == mode]
                info = summarize(subset)
                info.update({"scene": scene, "n": n, "method": mode,
                             "correctly_cross_verified": sum(
                                 r["cross_method_verified"] == "1"
                                 for r in subset),
                             "brute_oracle_frames": sum(
                                 r["brute_oracle_checked"] == "1"
                                 for r in subset)})
                receipt["summaries"].append(info)
                print("[CPU]", stem, mode, info, flush=True)

            if a.gpu:
                for mode in ("grid", "exhaustive"):
                    # Exhaustive GPU N^2 mode is run only while the full
                    # test count is within budget, even at 1M input scale.
                    if mode == "exhaustive" and (
                        n > 32768 or n*(n-1)//2 > a.max_tests
                    ):
                        continue
                    if mode == "grid" and scene == "all_overlap" and n > 4096:
                        # Serial hot-cell GPU path would leave one shader
                        # thread with N^2 work; the exhaustive GPU branch
                        # measures the true quadratic work in parallel.
                        continue
                    if time.monotonic() - started > a.suite_timeout:
                        receipt["stopped_reason"] = "Whole-suite wall-time budget"
                        break
                    dest = a.output / (stem + "-metal-" + mode + ".csv")
                    run = process([
                        str(metal), str(dest), scene, str(n), str(frames),
                        str(a.seed), mode, str(a.max_tests),
                        str(a.max_frame_ms), str(a.max_grid_entries),
                        str(a.max_gpu_mib),
                    ], a.output / "logs" / (stem + "-metal-" + mode + ".log"),
                        min(a.stage_timeout, max(30, int(a.suite_timeout - (
                            time.monotonic() - started)))))
                    receipt["runs"].append(run)
                    if run["outcome"] != "completed":
                        receipt["summaries"].append({
                            "scene": scene, "n": n,
                            "method": "metal_" + mode, "status": run["outcome"]
                        })
                        continue
                    vals = read(dest)
                    actual = summarize(vals, complete="COMPLETE",
                                       field="gpu_execution_ms")
                    actual.update({
                        "scene": scene, "n": n,
                        "method": "metal_" + mode,
                        "verified_frames": sum(
                            r["validation"] in {
                                "GPU_CPU_SMALL_BRUTE", "GPU_CPU_GRID_DIGEST",
                                "ANALYTIC_ALL_OVERLAP_COUNT_ONLY"
                            } for r in vals
                        ),
                    })
                    receipt["summaries"].append(actual)
                    print("[GPU]", stem, mode, actual, flush=True)

            if a.vulkan and n <= 32768 and n*(n-1)//2 <= a.max_tests:
                dest = a.output / (stem + "-vulkan-exhaustive.csv")
                run = process([
                    str(vulkan), str(dest), scene, str(n), str(frames),
                    str(a.seed), str(a.max_tests), str(a.max_frame_ms),
                    str(a.max_grid_entries),
                ], a.output / "logs" / (stem + "-vulkan-exhaustive.log"),
                    min(a.stage_timeout, max(30, int(a.suite_timeout - (
                        time.monotonic() - started)))))
                receipt["runs"].append(run)
                if run["outcome"] != "completed":
                    receipt["summaries"].append({
                        "scene": scene, "n": n,
                        "method": "vulkan_exhaustive",
                        "status": run["outcome"]
                    })
                else:
                    vals = read(dest)
                    actual = summarize(vals, complete="COMPLETE",
                                       field="gpu_timestamp_ms")
                    actual.update({
                        "scene": scene, "n": n,
                        "method": "vulkan_exhaustive",
                        "device_type": vals[0]["device_type"] if vals else "UNKNOWN",
                        "device": vals[0]["vulkan_device"] if vals else "UNKNOWN",
                        "verified_frames": sum(
                            r["validation"] in {
                                "VULKAN_CPU_BRUTE", "VULKAN_CPU_GRID_DIGEST",
                                "ANALYTIC_ALL_OVERLAP_COUNT_ONLY"
                            } for r in vals
                        )
                    })
                    receipt["summaries"].append(actual)
                    print("[VULKAN]", stem, actual, flush=True)
    finally:
        (a.output / "EXPERIMENT.json").write_text(
            json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
        )
        header = [
            "# Real global broad-phase pressure ladder — " + a.tier,
            "",
            f"Platform: {receipt['host']}, installed RAM: {memory} GiB.",
            f"Actual Metal requested: {'YES' if a.gpu else 'NO'}",
            f"Actual Vulkan requested: {'YES' if a.vulkan else 'NO'} "
            "(device type in Vulkan CSV; SOFTWARE_CPU is NOT hardware GPU)",
            "",
            "This is adversarial **global synthetic AABB** search—not proven "
            "isolated islands, 1M deformable worlds or physically resolved contacts.",
            "In GPU runs, pair materialization is deliberately replaced by "
            "exact search/count and two GPU/CPU pair fingerprints. "
            "This DOES NOT measure output writes for enormous candidate sets.",
            "",
            "| Scene | Bodies | Method | Complete frames | Mean query/kernel ms | "
            "P95 ms | Most tests on one frame | Last complete candidate count |",
            "|---|---:|---|---:|---:|---:|---:|---:|",
        ]
        for item in receipt["summaries"]:
            if "mean_ms" not in item:
                header.append(
                    f"| {item['scene']} | {item['n']:,} | "
                    f"{item['method']} **{item.get('status','failed')}** "
                    "| — | — | — | — | — |"
                )
                continue
            header.append(
                f"| {item['scene']} | {item['n']:,} | {item['method']} | "
                f"{item['complete_frames']}/{item['total_frames']} | "
                f"{item['mean_ms']:.4f} | {item['p95_ms']:.4f} | "
                f"{item['max_tested']:,} | "
                f"{item['last_complete_count']:,} |"
            )
        header += [
            "",
            "Incomplete or capped frames are excluded from measured means, "
            "and are still present in the original CSV. They must not be "
            "interpreted as correct finished candidate counts.",
            "",
            "Wall-time limits protect the local workstation from unbounded "
            "benchmark jobs; this does not prove full hardware saturation, "
            "maximum memory bandwidth or peak sustained GPU power.",
        ]
        markdown = "\n".join(header) + "\n"
        (a.output / "RESULTS.md").write_text(markdown, encoding="utf-8")
        table = "\n".join(
            "<tr>" + "".join(f"<td>{html.escape(v)}</td>" for v in
              [str(x.get("scene")), str(x.get("n")),
               str(x.get("method")), str(x.get("complete_frames","—"))+"/"+
                   str(x.get("total_frames","—")),
               str(x.get("mean_ms","—")),str(x.get("p95_ms","—")),
               str(x.get("max_tested","—")),str(x.get("last_complete_count","—"))])
            + "</tr>" for x in receipt["summaries"]
        )
        page = ("<!doctype html><html lang='en'><meta charset='utf-8'>"
                "<title>Kyoto Global Broadphase Pressure</title>"
                "<style>body{background:#101923;color:#e8f0f7;font:15px system-ui;"
                "margin:2em auto;max-width:1400px}h1{font-size:25px}"
                "table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}"
                "td,th{padding:10px;border-bottom:1px solid #3a4859;text-align:left}"
                "tr:hover{background:#1f2f40}.hint{color:#acbfd0;line-height:1.6}"
                "</style><h1>Kyoto: actual global collision-search pressure</h1>"
                "<p class='hint'>No separated-island shortcut. Two distinct CPU "
                "searches, optionally two real Metal kernels and one genuine Vulkan SPIR-V "
                "compute backend (which may be software-backed). Count-only "
                "not full contact physics. A capped row is NOT a completed solution.</p>"
                "<table><thead><tr><th>Scene</th><th>Objects</th><th>Method</th>"
                "<th>Completed frames</th><th>Mean ms</th><th>P95 ms</th>"
                "<th>Maximum visited pairs</th><th>Last exact candidate count</th>"
                "</tr></thead><tbody>" + table + "</tbody></table>"
                "<p class='hint'>Read RESULTS.md and each immutable CSV for "
                "the verification status, work cap, input memory and device timings.</p>"
                "</html>")
        (a.output / "index.html").write_text(page, encoding="utf-8")
        print(markdown, flush=True)
    return 0 if not any(x["outcome"] != "completed"
                       for x in receipt["runs"]) else 2


if __name__ == "__main__":
    raise SystemExit(main())
