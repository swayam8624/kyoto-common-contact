#!/usr/bin/env python3
"""Inspect coupled multi-cloth CBP topology replays; optional offline HTML.

Inputs are per-frame independent algorithm runs on ONE source physics
trajectory. This tool does NOT estimate confidence intervals across seeds,
measure end-to-end solver performance, or imply different rendered physics.
No external Python packages, CDN, or JavaScript libraries required.
"""
from __future__ import annotations

import argparse
import collections
import csv
import html
import math
from pathlib import Path
import statistics

METHODS = [
    "frozen_v4", "topology_exhaustive", "topology_xsweep",
    "topology_shared_bvh", "topology_certified_cache", "causal_cbp",
    "portfolio_v4_regional", "fresh_sap",
]
# Preserve the original seven-policy files as immutable evidence.
OLD_METHODS = set(METHODS) - {"topology_xsweep"}


def read_replay(path: Path) -> tuple[dict, list[dict]]:
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError(f"{path}: empty replay")
    required = {
        "frame", "snapshot_hash", "method", "query_ms", "pairs", "oracle_equal",
        "sheets", "spheres", "patch_span", "triangles", "primitives",
        "regions", "portfolio_choice", "portfolio_trial",
        "portfolio_accepted", "portfolio_rejected",
    }
    if required - rows[0].keys():
        raise ValueError(f"{path}: missing fields {sorted(required - rows[0].keys())}")
    frames: dict[int, dict[str, dict]] = collections.defaultdict(dict)
    for row in rows:
        f = int(row["frame"])
        method = row["method"]
        t = float(row["query_ms"])
        if method not in METHODS or method in frames[f]:
            raise ValueError(f"{path}: invalid or duplicate method/frame")
        if not math.isfinite(t) or t < 0 or row["oracle_equal"] != "1":
            raise ValueError(f"{path}: invalid timing or candidate parity")
        frames[f][method] = row
    policies = set(frames[1]) if 1 in frames else set()
    if policies not in (set(METHODS), OLD_METHODS):
        raise ValueError(f"{path}: unrecognized policy set")
    for frame, values in frames.items():
        if set(values) != policies:
            raise ValueError(f"{path}: incomplete frame {frame}")
        if len({v["pairs"] for v in values.values()}) != 1:
            raise ValueError(f"{path}: mismatched candidate counts")
        if len({v["snapshot_hash"] for v in values.values()}) != 1:
            raise ValueError(f"{path}: method-level geometry mismatch")
    ordered = sorted(frames)
    if ordered != list(range(1, max(ordered) + 1)):
        raise ValueError(f"{path}: missing/reordered frames")
    # Exclude a fixed six-frame startup window, matching earlier 48-frame
    # pilots. Preserve all transitions after warmup, including paid trials.
    warmup = min(6, max(1, len(ordered) // 8))
    timed = [frame for frame in ordered if frame > warmup]
    active_methods = [method for method in METHODS if method in policies]
    sums = {
        method: sum(float(frames[frame][method]["query_ms"]) for frame in timed)
        for method in active_methods
    }
    means = {method: sums[method] / len(timed) for method in active_methods}
    v4 = means["frozen_v4"]
    if v4 <= 0 or any(v <= 0 for v in means.values()):
        raise ValueError(f"{path}: zero-duration method cannot form speedup")
    rows_out = []
    for method in active_methods:
        values = [float(frames[f][method]["query_ms"]) for f in timed]
        sorted_values = sorted(values)
        p95 = sorted_values[math.ceil(.95 * len(values)) - 1]
        rows_out.append({
            "method": method, "mean": means[method], "p95": p95,
            "ratio": v4 / means[method], "sum": sums[method],
        })
    p = [frames[frame]["portfolio_v4_regional"] for frame in timed]
    choices = collections.Counter(item["portfolio_choice"] for item in p)
    result = {
        "path": path, "sheets": int(rows[0]["sheets"]),
        "spheres": int(rows[0]["spheres"]),
        "triangles": int(rows[0]["triangles"]),
        "primitives": int(rows[0]["primitives"]),
        "span": int(rows[0]["patch_span"]),
        "regions": int(rows[0]["regions"]),
        "warmup": warmup, "frames": len(timed),
        "summary": rows_out,
        "choices": choices,
        "accepted": sum(int(item["portfolio_accepted"]) for item in p),
        "rejected": sum(int(item["portfolio_rejected"]) for item in p),
        "frame_rows": [frames[f] for f in timed],
        "fingerprints": [
            frames[f]["fresh_sap"]["snapshot_hash"] for f in ordered
        ],
    }
    return result, rows



def temporal_windows(info: dict) -> list[dict]:
    """Diagnostic, hindsight-only local performance windows.

    Fixed-length windows are established by frame count (not retrospectively
    chosen winning frames). Reference and alternatives share the same window.
    """
    frames = info["frame_rows"]
    size = max(1, math.ceil(len(frames) / 4))
    windows = []
    for start in range(0, len(frames), size):
        group = frames[start:start + size]
        def mean(method: str) -> float:
            return statistics.fmean(float(frame[method]["query_ms"]) for frame in group)
        v4 = mean("frozen_v4")
        exhaustive = mean("topology_exhaustive")
        portfolio = mean("portfolio_v4_regional")
        sweep = mean("topology_xsweep") if "topology_xsweep" in group[0] else None
        windows.append({
            "begin": int(group[0]["frozen_v4"]["frame"]),
            "end": int(group[-1]["frozen_v4"]["frame"]),
            "v4": v4,
            "exhaustive": exhaustive,
            "portfolio": portfolio,
            "exhaustive_speedup": v4 / exhaustive if exhaustive else math.nan,
            "portfolio_speedup": v4 / portfolio if portfolio else math.nan,
            "sweep_speedup": v4 / sweep if sweep else None,
            "trial_frames": sum(int(x["portfolio_v4_regional"]["portfolio_trial"])
                                for x in group),
            "pairs": statistics.fmean(int(x["frozen_v4"]["pairs"]) for x in group),
        })
    return windows


def svg_crossover(info: dict) -> str:
    """Actual per-frame ratio, NOT a deployable hindsight selection policy."""
    data = info["frame_rows"]
    width, height = 750, 230
    x0, x1, y0, y1 = 56, 724, 25, 181
    variants = [
        ("topology_exhaustive", "#49bbaa"),
        ("portfolio_v4_regional", "#d6a56e"),
    ]
    if "topology_xsweep" in data[0]:
        variants.insert(1, ("topology_xsweep", "#89a7ed"))
    observed = [
        float(frame["frozen_v4"]["query_ms"]) /
        max(float(frame[mode]["query_ms"]), 1e-12)
        for frame in data for mode, _ in variants
    ]
    max_y = max(1.3, min(5.0, max(observed) * 1.1))
    def xpos(i: int) -> float:
        return x0 + (x1 - x0) * i / max(1, len(data) - 1)
    def ypos(value: float) -> float:
        return y1 - (y1 - y0) * min(max(value, 0.), max_y) / max_y
    shapes = [
        f'<line x1="{x0}" y1="{ypos(1):.1f}" x2="{x1}" '
        f'y2="{ypos(1):.1f}" stroke="#bccbd6" '
        'stroke-width="1" stroke-dasharray="4 5"/>',
        f'<text x="10" y="{ypos(1)+4:.1f}" font-size="12" fill="#cad2dc">1×</text>',
        f'<text x="{x0}" y="15" font-size="12" fill="#dce7f2">'
        'Frame-by-frame V4 speedup (above 1× = faster than V4)</text>',
    ]
    for mode, color in variants:
        points = []
        for i, frame in enumerate(data):
            baseline = float(frame["frozen_v4"]["query_ms"])
            candidate = float(frame[mode]["query_ms"])
            points.append(f"{xpos(i):.1f},{ypos(baseline / max(candidate,1e-12)):.1f}")
        shapes.append(
            f'<polyline points="{" ".join(points)}" fill="none" '
            f'stroke="{color}" stroke-width="2" stroke-linejoin="round"/>'
        )
    for i, (mode, color) in enumerate(variants):
        shapes.append(
            f'<rect x="{x0+220*i}" y="201" width="11" height="11" fill="{color}"/>'
            f'<text x="{x0+15+220*i}" y="211" font-size="11" '
            f'fill="#dce7f2">{html.escape(mode)}</text>'
        )
    shapes.append(
        f'<text x="{x0}" y="195" font-size="10" fill="#9eacbb">'
        f'Frames {data[0]["frozen_v4"]["frame"]}–'
        f'{data[-1]["frozen_v4"]["frame"]}; ratios clipped at {max_y:.1f}×</text>'
    )
    return (f'<svg viewBox="0 0 {width} {height}" role="img" '
            'aria-label="Measured per-frame speedups, not online decisions">'
            + "".join(shapes) + '</svg>')

def svg_bars(info: dict) -> str:
    # Plot ratio >1 as a faster-than-V4 result, <1 as a regression.
    data = info["summary"]
    top = max(1.15, max(item["ratio"] for item in data) * 1.10)
    width, left, plot = 750, 190, 515
    height = 38 * len(data) + 52
    rows = []
    axis_x = left + plot / top
    rows.append(
        f'<line x1="{axis_x:.1f}" x2="{axis_x:.1f}" y1="8" '
        f'y2="{height - 30}" stroke="#8495a9" stroke-dasharray="4 4"/>'
    )
    for index, item in enumerate(data):
        y = 13 + index * 38
        w = plot * item["ratio"] / top
        fill = "#47b59e" if item["ratio"] >= 1 else "#d28b72"
        rows.append(
            f'<text x="5" y="{y + 18}" fill="#dbe5ee" font-size="12">'
            f'{html.escape(item["method"])}</text>'
            f'<rect x="{left}" y="{y}" width="{w:.1f}" height="23" '
            f'rx="4" fill="{fill}"/>'
            f'<text x="{min(width - 36, left + w + 7):.1f}" y="{y + 17}" '
            f'font-size="11" fill="#f4f6fa">{item["ratio"]:.3f}×</text>'
        )
    return (
        f'<svg viewBox="0 0 {width} {height}" role="img" '
        f'aria-label="Method speedups relative to frozen V4">'
        + "".join(rows)
        + f'<text x="{left}" y="{height - 7}" fill="#8495a9" font-size="11">'
        + 'Speedup relative to frozen V4 — higher is better</text></svg>'
    )


def svg_timeline(info: dict) -> str:
    rows = info["frame_rows"]
    width = 750
    bar_width = (width - 28) / len(rows)
    colors = {
        "frozen_v4": "#7189b5",
        "region_exhaustive": "#47b59e",
        "region_shared_bvh": "#c49ce3",
        "region_certified_cache": "#d49a6b",
    }
    rects = []
    for i, frame in enumerate(rows):
        p = frame["portfolio_v4_regional"]
        mode = p["portfolio_choice"]
        fill = colors.get(mode, "#a0a0a0")
        rects.append(
            f'<rect x="{14 + i * bar_width:.2f}" y="10" '
            f'width="{max(.8, bar_width - .9):.2f}" height="32" '
            f'fill="{fill}"><title>Frame {p["frame"]}: '
            f'{html.escape(mode)} / {p["query_ms"]}ms</title></rect>'
        )
    return (
        f'<svg viewBox="0 0 {width} 58" role="img" '
        f'aria-label="Actual portfolio mode each timed frame">'
        + "".join(rects)
        + '</svg>'
    )


def html_report(data: list[dict]) -> str:
    sections = []
    for d in data:
        title = (
            f'{d["sheets"]} cloth sheets · {d["spheres"]} spheres · '
            f'patch span {d["span"]}'
        )
        rows = "".join(
            "<tr><td>" + html.escape(item["method"]) + "</td>"
            f'<td>{item["mean"]:.6f}</td><td>{item["p95"]:.6f}</td>'
            f'<td>{item["ratio"]:.3f}×</td></tr>'
            for item in d["summary"]
        )
        modes = ", ".join(
            f'{html.escape(key)}: {value} frames'
            for key, value in d["choices"].items()
        )
        sections.append(
            '<section><header><h2>' + title + '</h2>'
            f'<p>{d["triangles"]} mesh triangles, {d["primitives"]} '
            f'primitives, {d["regions"]} true topology-derived regions, '
            f'{d["frames"]} timed frames after {d["warmup"]} warmup</p>'
            '</header><h3>Query-time ratio vs V4</h3>'
            + svg_bars(d)
            + '<h3>How the winner changes over time — retrospective only</h3>'
            + svg_crossover(d)
            + '<h3>Fixed chronological windows (no cherry-picking)</h3>'
            + '<table><thead><tr><th>Frames</th><th>Mean pairs</th>'
            '<th>Fixed exhaustive / V4</th><th>Portfolio / V4</th>'
            '<th>Trial frames</th></tr></thead><tbody>'
            + "".join(
                f'<tr><td>{w["begin"]}–{w["end"]}</td>'
                f'<td>{w["pairs"]:.1f}</td>'
                f'<td>{w["exhaustive_speedup"]:.3f}×</td>'
                f'<td>{w["portfolio_speedup"]:.3f}×</td>'
                f'<td>{w["trial_frames"]}</td></tr>'
                for w in temporal_windows(d)
            ) + '</tbody></table>'
            + '<h3>Portfolio decisions (not hindsight)</h3>'
            + svg_timeline(d)
            + '<p class="muted">' + modes
            + f'. Accepted trials: {d["accepted"]}; '
            f'rejected trials: {d["rejected"]}.</p>'
            + '<h3>Measured timings</h3><table><thead><tr>'
            '<th>Method</th><th>Mean ms</th><th>P95 ms</th>'
            '<th>V4 / method</th></tr></thead><tbody>'
            + rows + '</tbody></table>'
            + f'<p class="muted">Input: {html.escape(d["path"].name)}</p>'
            + '</section>'
        )
    return (
        '<!doctype html><html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        '<title>Kyoto CBP — Topology-Aware Replay Evidence</title>'
        '<style>body{font:15px/1.55 system-ui,sans-serif;max-width:1100px;'
        'margin:32px auto;padding:0 20px;background:#111b27;color:#e5edf6}'
        'h1{font-size:30px;letter-spacing:-.03em}h2{font-size:23px}'
        'h3{font-size:16px;margin-top:26px}section{background:#192739;'
        'border:1px solid #36485d;padding:26px;border-radius:14px;'
        'margin:28px 0}svg{width:100%;height:auto;display:block}'
        'table{width:100%;border-collapse:collapse}td,th{padding:9px;'
        'text-align:left;border-bottom:1px solid #37465b}'
        '.muted{color:#9baebf;font-size:13px}'
        '.note{border-left:3px solid #c69568;padding:12px 16px;'
        'background:#253344}</style></head><body>'
        '<h1>Common Broad Phase — topology-based evidence</h1>'
        '<p class="note">Each replay runs seven or eight exact methods on the '
        '<em>same physical trajectory</em>. Lower query time is better. '
        'All candidate pairs matched, but different methods do not generate '
        'separate physical cloth animations here. These are short developer '
        'replays, without statistical replication, CCD proof, or end-to-end '
        'solver acceleration claims.</p>'
        + "".join(sections)
        + '<p class="muted">Offline, dependency-free, generated locally. '
        'Never interpret a single observed ratio as a confidence bound.</p>'
        + '</body></html>'
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", nargs="+", type=Path)
    parser.add_argument("--html", type=Path, help="New HTML output path")
    args = parser.parse_args()
    datasets = [read_replay(path)[0] for path in args.csv]
    cohorts = collections.defaultdict(list)
    for data in datasets:
        key = (
            data["sheets"], data["spheres"], data["triangles"],
            len(data["fingerprints"]),
        )
        cohorts[key].append(data)
    for group in cohorts.values():
        if len(group) <= 1:
            continue
        reference = group[0]["fingerprints"]
        for data in group[1:]:
            if data["fingerprints"] != reference:
                raise ValueError(
                    "Physics snapshots differ across patch spans: "
                    f'{group[0]["path"]} vs {data["path"]}'
                )
    print("# Kyoto CBP topology replay summary")
    print()
    print("| Source | Frozen V4 mean ms | Portfolio mean ms | "
          "Portfolio speedup | Exactness |")
    print("|---|---:|---:|---:|---|")
    for dataset in datasets:
        item = {v["method"]: v for v in dataset["summary"]}
        print(f'| {dataset["path"].name} | '
              f'{item["frozen_v4"]["mean"]:.6f} | '
              f'{item["portfolio_v4_regional"]["mean"]:.6f} | '
              f'{item["portfolio_v4_regional"]["ratio"]:.3f}× | '
              'pair parity verified |')
    print()
    for group in cohorts.values():
        if len(group) > 1:
            print(
                f'Identical frame geometry verified across {len(group)} patch '
                f'spans for {group[0]["sheets"]} cloth sheets / '
                f'{group[0]["spheres"]} moving spheres.'
            )

    for dataset in datasets:
        print()
        print(
            f'## All methods — {dataset["path"].name}: '
            f'{dataset["regions"]} regions, {dataset["frames"]} timed frames'
        )
        print()
        print("| Method | Mean query ms | P95 ms | V4 / method |")
        print("|---|---:|---:|---:|")
        for item in dataset["summary"]:
            print(
                f'| {item["method"]} | {item["mean"]:.6f} | '
                f'{item["p95"]:.6f} | {item["ratio"]:.3f}× |'
            )
        modes = ", ".join(
            f"{name}={count}" for name, count in sorted(dataset["choices"].items())
        )
        print(
            f'Portfolio: {modes}; accepted={dataset["accepted"]}; '
            f'rejected={dataset["rejected"]}.'
        )
        print("Retrospective fixed chronological windows (V4 / alternative; >1× faster):")
        for w in temporal_windows(dataset):
            swept = (
                f'{w["sweep_speedup"]:.3f}×' if w["sweep_speedup"] is not None
                else "n/a (legacy 7-policy trace)"
            )
            print(
                f'  frames {w["begin"]:>2}-{w["end"]:>2} | '
                f'pairs/frame {w["pairs"]:.1f} | '
                f'fixed exhaustive {w["exhaustive_speedup"]:.3f}× | '
                f'X-sweep {swept} | '
                f'portfolio {w["portfolio_speedup"]:.3f}× | '
                f'paid trial frames {w["trial_frames"]}'
            )


    print("All results are descriptive; no cross-seed significance is implied.")
    if args.html:
        if args.html.exists():
            raise FileExistsError(f"Refusing to overwrite {args.html}")
        args.html.parent.mkdir(parents=True, exist_ok=True)
        args.html.write_text(html_report(datasets), encoding="utf-8")
        print(f"Offline report: {args.html}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
