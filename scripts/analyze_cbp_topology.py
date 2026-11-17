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
    "frozen_v4", "topology_exhaustive", "topology_shared_bvh",
    "topology_certified_cache", "causal_cbp", "portfolio_v4_regional",
    "fresh_sap",
]


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
    for frame, values in frames.items():
        if set(values) != set(METHODS):
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
    sums = {
        method: sum(float(frames[frame][method]["query_ms"]) for frame in timed)
        for method in METHODS
    }
    means = {method: sums[method] / len(timed) for method in METHODS}
    v4 = means["frozen_v4"]
    if v4 <= 0 or any(v <= 0 for v in means.values()):
        raise ValueError(f"{path}: zero-duration method cannot form speedup")
    rows_out = []
    for method in METHODS:
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
        '<p class="note">Each replay runs seven exact methods on the '
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
