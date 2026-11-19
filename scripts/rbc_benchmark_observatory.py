#!/usr/bin/env python3
"""Kyoto RBC benchmark observatory: terminal tables + offline interactive charts.

Consumes existing NATIVE data; never invents benchmark observations.
The optional Stage-2 RBC_RESULTS.json covers materials, budgets, motion
regimes, broad-phase policies and physically coupled solver scenarios.
The optional native five-policy CSV covers synchronized real solver frames.

Examples:
  python3 scripts/rbc_benchmark_observatory.py \
    --research output/rbc-stage2-01/RBC_RESULTS.json \
    --proof output/rbc-clean-physics-mac-03/NATIVE_FIVE_POLICY_SOLVER.csv \
    --output output/rbc-observatory-01
  python3 scripts/rbc_benchmark_observatory.py \
    --proof output/rbc-clean-physics-mac-03/NATIVE_FIVE_POLICY_SOLVER.csv \
    --output output/rbc-observatory-proof-only-01

Only Python's standard library is required. The HTML is entirely offline
and has no npm, CDN, esbuild, Python plotting, or WebGL dependency.
The rotatable 3-D surface represents measured SPEEDUP, not invented physics.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

MATERIAL_METHODS = (
    "sap_rebuild", "sap_persistent", "guard_fixed_exact",
    "v4_frozen", "rbc_indexed_repair", "rbc_flat_repair",
)
PHYSICAL_METHODS = (
    "sap_rebuild", "global_guard", "frozen_v4", "rbc_indexed", "rbc_flat",
)
BASELINE = "guard_fixed_exact"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def number(value, field: str, *, positive: bool = False) -> float:
    try:
        n = float(value)
    except (ValueError, TypeError) as exc:
        raise ValueError(f"Missing/non-numeric {field}: {value!r}") from exc
    if not math.isfinite(n) or (positive and n <= 0) or n < 0:
        raise ValueError(f"Invalid native {field}: {value!r}")
    return n


def geomean(values: list[float]) -> float:
    if not values or any(not math.isfinite(x) or x <= 0 for x in values):
        raise ValueError("Cannot aggregate an empty/nonpositive ratio cohort")
    return math.exp(statistics.mean(math.log(x) for x in values))


def locate(path: str, manifest: Path, subfolder: str) -> Path:
    candidate = Path(path)
    options = [
        candidate, manifest.parent / candidate,
        manifest.parent / subfolder / candidate.name,
    ]
    for item in options:
        if item.is_file():
            return item.resolve()
    raise FileNotFoundError(f"Missing native source: {path} (from {manifest})")


def read_csv(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError(f"Empty original native CSV: {path}")
    return rows


def read_research(path: Path) -> dict:
    doc = json.loads(path.read_text(encoding="utf-8"))
    if doc.get("schema") != "kyoto-rbc-stage2-real-native-evidence-v1":
        raise ValueError("Not an original Stage-2 RBC_RESULTS.json")
    scenes = doc.get("native_material_scenes", [])
    coupled = doc.get("coupled_physical_scenes", [])
    if not scenes:
        raise ValueError("RBC research manifest has no material comparisons")
    material = []
    for item in scenes:
        result = item["result"]
        csv_path = locate(result["source"], path, "material_csv")
        if sha256(csv_path) != result["sha256"]:
            raise ValueError(f"Native material CSV hash changed: {csv_path}")
        actual_rows = read_csv(csv_path)
        methods = defaultdict(set)
        for row in actual_rows:
            method = row["method"]
            if method not in MATERIAL_METHODS:
                raise ValueError(f"Unknown method in {csv_path}: {method}")
            key = int(row["frame"])
            if key in methods[method]:
                raise ValueError(f"Duplicate native method/frame in {csv_path}")
            methods[method].add(key)
            number(row["query_ms"], "query_ms", positive=True)
            if int(row["after_warmup"]) not in (0, 1):
                raise ValueError("Invalid native warmup marker")
        if set(methods) != set(MATERIAL_METHODS):
            raise ValueError("Native scene is missing a compared method")
        if any(methods[m] != methods[BASELINE] for m in MATERIAL_METHODS):
            raise ValueError("Different native timing frames across methods")
        # The manifest flag alone is not an oracle. Cross-check full
        # candidate counts and pair fingerprints on EVERY original frame.
        pair_receipts = defaultdict(set)
        for row in actual_rows:
            pair_receipts[int(row["frame"])].add(
                (int(row["pairs"]), int(row["pair_fnv64"])))
        if any(len(values) != 1 for values in pair_receipts.values()):
            raise ValueError(f"Native pair identity differs across methods: {csv_path}")
        if not result["all_five_pair_streams_equal_fresh_sap_on_every_frame"]:
            raise ValueError("Native exact-pair parity did not pass")
        means = result["per_method_mean_ms"]
        if set(means) != set(MATERIAL_METHODS):
            raise ValueError("Different methods in native result summary")
        # Recalculate AFTER warmup independently from source CSV.
        expected_warmup = int(result["warmup"])
        recomputed = {}
        for method in MATERIAL_METHODS:
            frames = [r for r in actual_rows
                      if r["method"] == method and int(r["frame"]) >= expected_warmup]
            if not frames:
                raise ValueError("Native scene contains no timed frames")
            recomputed[method] = statistics.mean(
                number(r["query_ms"], "query_ms", positive=True) for r in frames)
            if not math.isclose(recomputed[method], float(means[method]),
                                rel_tol=1e-7, abs_tol=1e-8):
                raise ValueError(f"Native time summary differs from CSV: {method}")
        material.append({
            "material": item["material"],
            "regime": item["regime"],
            "budget": int(item["budget"]),
            "n": int(item["actual_n"]),
            "means": recomputed,
            "source": str(csv_path),
        })
    physical = []
    for item in coupled:
        result = item["result"]
        csv_path = locate(result["source"], path, "coupled_csv")
        if sha256(csv_path) != result["sha256"]:
            raise ValueError(f"Coupled native CSV hash changed: {csv_path}")
        rows = read_csv(csv_path)
        methods = defaultdict(set)
        for row in rows:
            policy = row["policy"]
            if policy not in PHYSICAL_METHODS:
                raise ValueError("Unknown physical solver policy")
            frame = int(row["frame"])
            if frame in methods[policy]:
                raise ValueError("Repeated physical solver policy/frame")
            methods[policy].add(frame)
            number(row["step_ms"], "full_solver_step_ms")
            number(row["broad_ms"], "broadphase_ms")
            if int(row["oracle_misses"]) != 0:
                raise ValueError("Original native physical candidate oracle miss")
        if set(methods) != set(PHYSICAL_METHODS):
            raise ValueError("Physical solver scene has incomplete policies")
        if any(methods[m] != methods["sap_rebuild"] for m in PHYSICAL_METHODS):
            raise ValueError("Physical solver methods have different frames")
        if not result["all_sap_reference_particle_states_equal"]:
            raise ValueError("Coupled solver SAP state parity failed")
        means = result["solver_mean_full_step_ms_by_policy"]
        if set(means) != set(PHYSICAL_METHODS):
            raise ValueError("Incomplete coupled full-step summary")
        # Verify measured whole-step averages with the actual warmup rule.
        after = min(8, max(0, len(methods["sap_rebuild"]) // 8))
        for policy in PHYSICAL_METHODS:
            samples = [number(row["step_ms"], "full_solver_step_ms")
                       for row in rows
                       if row["policy"] == policy and int(row["frame"]) >= after]
            if not math.isclose(statistics.mean(samples), float(means[policy]),
                                rel_tol=1e-7, abs_tol=1e-8):
                raise ValueError(f"Coupled CSV/summary timing mismatch: {policy}")
        # Reuse the SAME verified native coupled CSV to drive frame plots.
        # These samples are measured solver states, not a second simulation.
        series = {policy: [] for policy in PHYSICAL_METHODS}
        for row in rows:
            record = {"frame": int(row["frame"])}
            for field in ("step_ms", "broad_ms", "narrow_ms",
                          "self_contacts", "sphere_contacts",
                          "self_crossings", "max_stretch"):
                record[field] = number(row[field], field)
            series[row["policy"]].append(record)
        for policy in PHYSICAL_METHODS:
            series[policy].sort(key=lambda frame: frame["frame"])
            if [f["frame"] for f in series[policy]] != list(range(len(series[policy]))):
                raise ValueError("Native coupled frame sequence incomplete")
        physical.append({
            "scenario": item["scenario"],
            "means": {m: number(means[m], "whole_solver_ms", positive=True)
                      for m in PHYSICAL_METHODS},
            "frames": len(methods["sap_rebuild"]),
            "series": series,
            "source": str(csv_path),
        })
    return {"scenes": material, "physical": physical,
            "manifest": str(path.resolve()), "sha256": sha256(path),
            "warnings": doc.get("warnings", []),
            "host": doc.get("host_platform", "unknown")}


def read_proof(path: Path) -> dict:
    rows = read_csv(path)
    by_policy = defaultdict(dict)
    policies = set(PHYSICAL_METHODS)
    for row in rows:
        method = row["policy"]
        if method not in policies:
            raise ValueError(f"Unexpected physical policy {method}")
        frame = int(row["frame"])
        if frame < 0 or frame in by_policy[method]:
            raise ValueError("Duplicate/negative native physical frame")
        fields = ("step_ms", "broad_ms", "narrow_ms", "self_contacts",
                  "sphere_contacts", "geometric_self_intersections",
                  "max_position_diff_sap")
        record = {"frame": frame}
        for name in fields:
            record[name] = number(row[name], name)
        if int(row["oracle_misses"]) != 0:
            raise ValueError("Native proof reports candidate oracle misses")
        if record["max_position_diff_sap"] > 1e-7:
            raise ValueError("Native proof particle-state parity failed")
        by_policy[method][frame] = record
    if set(by_policy) != policies:
        raise ValueError("Native five-policy proof missing policy")
    frames = sorted(by_policy["sap_rebuild"])
    if frames != list(range(len(frames))) or len(frames) < 2:
        raise ValueError("Native proof has missing/nonsequential frames")
    if any(sorted(by_policy[p]) != frames for p in policies):
        raise ValueError("Physical policies cover different frames")
    receipt = path.parent / "PHYSICAL_EVIDENCE_MANIFEST.json"
    if receipt.is_file():
        manifest = json.loads(receipt.read_text("utf-8"))
        if manifest.get("native_CSV_SHA256") != sha256(path):
            raise ValueError("Proof native CSV SHA256 differs from original receipt")
    reference = by_policy["sap_rebuild"]
    if any(reference[f]["geometric_self_intersections"] !=
           by_policy[p][f]["geometric_self_intersections"]
           for p in policies for f in frames):
        raise ValueError("Inconsistent original geometric crossing counts")
    return {
        "policies": [
            {"name": name, "frames": [by_policy[name][f] for f in frames]}
            for name in PHYSICAL_METHODS
        ],
        "frames": len(frames), "sha256": sha256(path),
        "source": str(path.resolve()),
        "viewer": str(path.parent / "INTERACTIVE_ORIGINAL_PHYSICS.html")
        if (path.parent / "INTERACTIVE_ORIGINAL_PHYSICS.html").is_file() else None,
        "mean_step": {
            p: statistics.mean(by_policy[p][f]["step_ms"]
                               for f in frames[8:]) if len(frames) > 8 else
               statistics.mean(by_policy[p][f]["step_ms"] for f in frames)
            for p in PHYSICAL_METHODS
        },
    }


def aggregate(scenes: list[dict]) -> list[dict]:
    result = []
    for m in MATERIAL_METHODS:
        ratios = [s["means"][BASELINE] / s["means"][m] for s in scenes]
        result.append({
            "method": m,
            "geomean_guard_speedup": geomean(ratios),
            "wins_vs_guard": sum(x > 1 for x in ratios),
            "scenes": len(ratios),
            "mean_query_ms": statistics.mean(s["means"][m] for s in scenes),
        })
    return sorted(result, key=lambda r: -r["geomean_guard_speedup"])


def table(title: str, headers: list[str], data: list[list[str]]) -> None:
    print("\n" + title)
    widths = [max(len(headers[i]), *(len(row[i]) for row in data))
              for i in range(len(headers))]
    fmt = "  ".join("{:<" + str(width) + "}" for width in widths)
    print(fmt.format(*headers))
    print("  ".join("-" * w for w in widths))
    for row in data:
        print(fmt.format(*row))


def html_document(payload: dict) -> str:
    raw = json.dumps(payload, separators=(",", ":"), allow_nan=False)
    raw = raw.replace("<", "\\u003c").replace(">", "\\u003e")
    return HTML.replace("__DATA__", raw)


HTML = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kyoto / RBC Research Observatory</title>
<style>
:root{color-scheme:dark;--bg:#0b1018;--panel:#161f2c;--edge:#2a3747;--text:#ebf1f7;--muted:#9bafbf;--good:#65d8c0;--bad:#f69c86}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at top,#1b2a3a 0%,var(--bg) 45%);color:var(--text);font:14px system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:1660px;margin:auto;padding:30px clamp(14px,3vw,42px) 80px}h1{font-size:clamp(30px,3vw,49px);letter-spacing:-.045em;line-height:1.02;margin:10px 0}
h2{font-size:19px;letter-spacing:-.02em;margin:0 0 8px}p{line-height:1.6;color:var(--muted)}.kicker{text-transform:uppercase;font-size:11px;color:var(--good);font-weight:800;letter-spacing:.20em}
header{padding:26px 0 28px;border-bottom:1px solid var(--edge)}.meta{color:var(--muted);max-width:940px}
.stats{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px;margin:20px 0}.stat,.card{background:linear-gradient(135deg,#182433,#121b27);border:1px solid var(--edge);border-radius:13px;padding:20px;min-width:0}
.stat span{display:block;color:var(--muted);font-size:11px;letter-spacing:.08em;text-transform:uppercase}.stat strong{display:block;font-size:28px;margin-top:10px;font-variant-numeric:tabular-nums}
.dashboard{display:grid;grid-template-columns:1fr 1fr;gap:14px}.wide{grid-column:1/-1}.under{font-size:12px;color:var(--muted);margin:0 0 16px}
canvas{width:100%;height:310px;display:block;background:#111b28;border:1px solid #243444;border-radius:9px}
#surface{height:490px;touch-action:none;cursor:grab}#surface:active{cursor:grabbing}
#coupled-lines{height:350px}#coupled-heat{height:220px}
#surface-caption{font-size:12px;line-height:1.6}
.subtle{font-size:12px;color:#aebfcd;margin-top:12px}
select{background:#213246;color:var(--text);border:1px solid #3d5367;border-radius:7px;padding:8px}
.controls{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin:12px 0 16px}.muted{color:var(--muted)}a{color:#a6e8dc}
table{border-collapse:collapse;width:100%;font-size:12px;font-variant-numeric:tabular-nums}th,td{border-bottom:1px solid #273848;text-align:right;padding:11px 9px}th:first-child,td:first-child{text-align:left}th{color:#a5bfcb;font-weight:650}
.scroll{overflow-x:auto}.barrow{display:grid;grid-template-columns:180px 1fr 77px;gap:12px;align-items:center;margin:12px 0;font-size:12px}.bartrack{height:14px;background:#263747;border-radius:5px;overflow:hidden}.barfill{height:100%;background:linear-gradient(90deg,#368ca4,#76e0c4)}
.heat{display:grid;gap:5px;align-items:stretch}.cell{border-radius:5px;text-align:center;display:grid;place-items:center;padding:10px 4px;min-height:49px;font-size:12px;font-weight:700}
.heading{color:var(--muted);font-size:11px;display:grid;place-items:center}.notice{border-left:3px solid #eab676;padding:12px 15px;background:#2c281f;color:#f0d9b1;margin-top:22px;line-height:1.65;font-size:12px}
footer{border-top:1px solid var(--edge);margin-top:32px;padding:20px 0;color:var(--muted);font-size:12px}
@media(max-width:900px){.dashboard{grid-template-columns:1fr}.wide{grid-column:auto}.stats{grid-template-columns:repeat(2,minmax(0,1fr))}.barrow{grid-template-columns:120px 1fr 65px}}
</style></head><body><main>
<header><div class="kicker">Kyoto collision research • native measured evidence</div><h1>Research Observatory</h1>
<div class="meta">Exact broad-phase candidate comparisons, original full-step CPU timings, physical diagnostics, and measurable performance surfaces. <strong>No fabricated frames or benchmark scores.</strong></div></header>
<section class="stats" id="stats"></section>
<div class="dashboard">
<section class="card wide" id="material-section">
<h2>Algorithm benchmark leaderboard</h2><p class="under">Full-pair native query times. Every speedup is relative to the same-scene fixed Guard baseline; geometric mean across scenes. Not a complete solver speedup.</p>
<div class="scroll"><table id="leaderboard"></table></div><div id="bars"></div>
</section>
<section class="card">
<h2>Measured speedup heatmap</h2><p class="under">Rows = material family; columns = requested object budget (actual native N may differ). Same-scene Guard / method; above 1× is faster. No cross-material interpolation.</p>
<div class="controls"><label>Regime <select id="regime"></select></label><label>Method <select id="method"></select></label></div>
<div id="heatmap"></div>
</section>
<section class="card">
<h2>3D performance atlas · discrete measured cohorts</h2><p class="under">Independent 3D columns. X = nominal object budget, Y = material category, Z = Guard / selected method speedup. 1× means parity. Drag to orbit; hover for native N. No invented physical simulation or continuity between categories.</p>
<canvas id="surface"></canvas><div class="muted" id="surfacecaption"></div>
</section>
<section class="card wide" id="physical-section">
<h2>Real coupled-solver comparisons</h2><p class="under">Measured whole-step CPU milliseconds including simulation and broad-phase. These measurements are not interchangeable with query-only speedups.</p>
<div class="scroll"><table id="physical"></table></div>
</section>
<section class="card wide" id="coupled-traces-section">
<h2>Real coupled-physics time series</h2><p class="under">Original 60-frame native solver observations from the same SHA-verified coupled CSV files used in the table above. These are actual timings, not geometry-proof meshes or repeated sampling sessions.</p>
<div class="controls"><label>Scenario <select id="coupled-scenario"></select></label><label>Metric <select id="coupled-metric">
<option value="step_ms">Whole solver step (ms)</option>
<option value="broad_ms">Broad phase (ms)</option>
<option value="narrow_ms">Narrow phase (ms)</option>
<option value="self_contacts">Solver self-contact corrections (count)</option>
<option value="sphere_contacts">Sphere contact corrections (count)</option>
<option value="max_stretch">Maximum edge stretch (ratio)</option>
</select></label></div>
<canvas id="coupled-lines"></canvas>
<h2 style="margin-top:22px">Frame × broad-phase policy heatmap</h2>
<p class="under">Each cell is an original native solver observation; warmup frames are shown, but excluded from reported means.</p>
<canvas id="coupled-heat"></canvas>
<div class="subtle">Important: contact correction counts are solver diagnostics, not a proof of zero nonlocal mesh penetrations or continuous collision detection.</div>
</section>
<section class="card wide" id="proof-section">
<h2>Five-policy synchronized physics evidence</h2><p class="under">Original C++ time series. The contact/intersection plot uses SAP reference alone to avoid counting the same physical trajectory five times.</p>
<div class="controls"><label>Metric <select id="metric"><option value="step_ms">Whole step (ms)</option><option value="broad_ms">Broad-phase (ms)</option><option value="narrow_ms">Narrow-phase (ms)</option><option value="self_contacts">Same-sheet contact corrections (count)</option><option value="sphere_contacts">Sphere contact corrections (count)</option><option value="geometric_self_intersections">Endpoint triangle intersections (count)</option></select></label>
<a id="viewer" href="#" style="display:none">Open actual original-geometry 3D viewer</a></div>
<canvas id="lines"></canvas>
<h2 style="margin-top:24px">Native frame × policy metric heatmap</h2><p class="under">Each cell is an original native timing or solver diagnostic. Darker/cooler = lower measured value, warmer = higher measured value.</p>
<canvas id="frameheat"></canvas>
<div id="correctness" class="notice"></div>
</section>
</div>
<div id="limits" class="notice"></div>
<footer>Source input hashes are written to OBSERVATORY_RECEIPTS.json. Chart render time is excluded from all native timing measurements. A zero endpoint-intersection count is NOT continuous collision detection or a calibrated physical-accuracy proof.</footer>
</main><script>
"use strict";
const data=__DATA__;
const $=id=>document.getElementById(id);
const text=(tag,value)=>{const x=document.createElement(tag);x.textContent=String(value);return x;};
function fmt(x,n=3){return Number.isFinite(x)?x.toFixed(n):"—";}
function card(k,v){let el=document.createElement("div");el.className="stat";el.append(text("span",k),text("strong",v));$("stats").append(el);}
function colorRatio(x){if(x===null)return "#263444";let delta=Math.max(-1,Math.min(1,Math.log(x)/.7));return delta>=0?"rgb("+Math.round(50+26*delta)+","+Math.round(93+109*delta)+","+Math.round(115+60*delta)+")":"rgb("+Math.round(83-(-delta)*2)+","+Math.round(92-(-delta)*41)+","+Math.round(108-(-delta)*48)+")";}
function setupCanvas(id){const el=$(id);const dpr=window.devicePixelRatio||1;const w=el.clientWidth,h=el.clientHeight;el.width=Math.max(300,Math.round(w*dpr));el.height=Math.max(100,Math.round(h*dpr));const c=el.getContext("2d");c.setTransform(dpr,0,0,dpr,0,0);return {c,w,h};}
function aggregate(values){return values.length?Math.exp(values.reduce((a,b)=>a+Math.log(b),0)/values.length):null;}
function matrix(){
  const scenes=data.research.scenes;
  const regime=$("regime").value, method=$("method").value;
  const filtered=scenes.filter(s=>s.regime===regime);
  const mats=[...new Set(filtered.map(s=>s.material))].sort(), budgets=[...new Set(filtered.map(s=>s.budget))].sort((a,b)=>a-b);
  const cells=mats.map(m=>budgets.map(b=>{
    const xs=filtered.filter(s=>s.material===m&&s.budget===b).map(s=>s.means.guard_fixed_exact/s.means[method]);
    return aggregate(xs);
  }));
  return {mats,budgets,cells,regime,method};
}
function drawHeat(){const z=matrix();const grid=$("heatmap");grid.replaceChildren();grid.className="heat";grid.style.gridTemplateColumns="115px repeat("+z.budgets.length+",minmax(0,1fr))";
  grid.append(text("div",""));z.budgets.forEach(b=>{const e=text("div",String(b)+" objects");e.className="heading";grid.append(e)});
  z.mats.forEach((m,i)=>{const h=text("div",m);h.className="heading";grid.append(h);z.budgets.forEach((b,j)=>{
    let v=z.cells[i][j];const el=text("div",v===null?"NO DATA":fmt(v,2)+"×");el.className="cell";el.style.background=colorRatio(v);el.title=m+" / "+b+": "+(v===null?"unmeasured":fmt(v,5)+" Guard/method");grid.append(el);
  });});$("surfacecaption").textContent="Regime: "+z.regime+" · Method: "+z.method+" · missing cells are not interpolated";drawSurface();
}
let yaw=-0.65,pitch=.48, dragging=false,lastX=0,lastY=0;
function drawSurface(){if(!data.research)return;const {c,w,h}=setupCanvas("surface"),z=matrix();
 c.clearRect(0,0,w,h);c.fillStyle="#111b28";c.fillRect(0,0,w,h);
 const rows=z.mats.length,cols=z.budgets.length,flat=z.cells.flat().filter(x=>x!==null);
 if(!flat.length){c.fillStyle="#b4c8d5";c.fillText("No measured cells",24,34);return;}
 const zz=Math.max(1.1,...flat),range=Math.max(.15,zz-.55),scale=Math.min(w*.68,h*.78);
 const transform=(x,y,v)=>{const X=(x/Math.max(1,cols-1)-.5)*2.3,Y=(y/Math.max(1,rows-1)-.5)*2.0,Z=((v-.55)/range)*1.5-.5;
 const X1=X*Math.cos(yaw)-Y*Math.sin(yaw), Y1=X*Math.sin(yaw)+Y*Math.cos(yaw);
 const Y2=Y1*Math.cos(pitch)-Z*Math.sin(pitch), Z2=Y1*Math.sin(pitch)+Z*Math.cos(pitch);
 return [w/2+X1*scale*.36,h*.60+Y2*scale*.34,Z2];};
 const quad=[];for(let i=0;i<rows-1;i++)for(let j=0;j<cols-1;j++){let vals=[z.cells[i][j],z.cells[i][j+1],z.cells[i+1][j+1],z.cells[i+1][j]];if(vals.some(v=>v===null))continue;
 let pts=[transform(j,i,vals[0]),transform(j+1,i,vals[1]),transform(j+1,i+1,vals[2]),transform(j,i+1,vals[3])];
 quad.push({pts,depth:pts.reduce((a,p)=>a+p[2],0)/4,value:aggregate(vals)});}
 quad.sort((a,b)=>a.depth-b.depth);
 quad.forEach(q=>{c.beginPath();q.pts.forEach((p,i)=>i?c.lineTo(p[0],p[1]):c.moveTo(p[0],p[1]));c.closePath();c.fillStyle=colorRatio(q.value);c.globalAlpha=.88;c.fill();c.globalAlpha=1;c.strokeStyle="#a4bbcc";c.lineWidth=.7;c.stroke();});
 for(let i=0;i<rows;i++)for(let j=0;j<cols;j++){let v=z.cells[i][j];if(v===null)continue;let p=transform(j,i,v);c.beginPath();c.arc(p[0],p[1],3,0,Math.PI*2);c.fillStyle="#f6edd4";c.fill();}
 c.fillStyle="#c5d9e3";c.font="12px system-ui";c.fillText("Z: Guard / method (×)",14,24);c.fillText("X: object budget →",14,h-18);c.fillText("Y: materials (axis rotated with surface)",14,h-38);
}
$("surface").addEventListener("pointerdown",e=>{dragging=true;lastX=e.clientX;lastY=e.clientY;$("surface").setPointerCapture(e.pointerId);});
$("surface").addEventListener("pointermove",e=>{if(!dragging)return;yaw+=(e.clientX-lastX)*.008;pitch=Math.max(-1.25,Math.min(1.25,pitch+(e.clientY-lastY)*.006));lastX=e.clientX;lastY=e.clientY;drawSurface();});
$("surface").addEventListener("pointerup",()=>dragging=false);
function buildLeaderboard(){if(!data.research){$("material-section").style.display="none";$("physical-section").style.display="none";return;}
 const r=data.summary,table=$("leaderboard");let head=document.createElement("tr");["Method","Guard / method GM","Mean query ms","Faster scenes","Measured scenes"].forEach(s=>head.append(text("th",s)));table.append(head);
 let max=Math.max(...r.map(x=>x.geomean_guard_speedup));
 r.forEach(v=>{const tr=document.createElement("tr");[v.method,fmt(v.geomean_guard_speedup,4)+"×",fmt(v.mean_query_ms,4),v.wins_vs_guard,v.scenes].forEach(x=>tr.append(text("td",x)));table.append(tr);
  const bar=document.createElement("div");bar.className="barrow";bar.append(text("span",v.method));
  const track=document.createElement("div");track.className="bartrack";const fill=document.createElement("div");fill.className="barfill";fill.style.width=100*v.geomean_guard_speedup/max+"%";track.append(fill);bar.append(track,text("span",fmt(v.geomean_guard_speedup,3)+"×"));$("bars").append(bar);
 });
 const regimes=[...new Set(data.research.scenes.map(s=>s.regime))].sort();
 regimes.forEach(m=>{const x=text("option",m);x.value=m;$("regime").append(x);});
 data.summary.forEach(x=>{const e=text("option",x.method);e.value=x.method;$("method").append(e);});
 $("method").value="rbc_indexed_repair";$("regime").addEventListener("change",drawHeat);$("method").addEventListener("change",drawHeat);drawHeat();
 const phys=$("physical");let h=document.createElement("tr");["Scenario",...data.physical_methods.map(s=>s+" (ms)")].forEach(s=>h.append(text("th",s)));phys.append(h);
 data.research.physical.forEach(s=>{const tr=document.createElement("tr");tr.append(text("td",s.scenario));data.physical_methods.forEach(m=>tr.append(text("td",fmt(s.means[m]))));phys.append(tr);});
 if(!data.research.physical.length)$("physical-section").style.display="none";
}
function linePlot(){if(!data.proof)return;const {c,w,h}=setupCanvas("lines"),p=data.proof,metric=$("metric").value,pad=48;
 c.clearRect(0,0,w,h);const colors=["#7ea9fa","#f5b879","#f2d78d","#73dbbd","#dc97cb"];
 const vals=p.policies.flatMap(x=>x.frames.map(f=>f[metric]));const top=Math.max(.001,...vals)*1.08;
 c.strokeStyle="#304459";c.fillStyle="#a7bacb";c.font="11px system-ui";
 for(let i=0;i<5;i++){const y=pad+(h-pad*1.7)*i/4;c.beginPath();c.moveTo(pad,y);c.lineTo(w-22,y);c.stroke();c.fillText(fmt(top*(1-i/4),2),4,y+4);}
 p.policies.forEach((s,j)=>{c.strokeStyle=colors[j];c.lineWidth=2;c.beginPath();s.frames.forEach((f,i)=>{let x=pad+i*(w-pad-22)/Math.max(1,s.frames.length-1),y=pad+(1-f[metric]/top)*(h-pad*1.7);i?c.lineTo(x,y):c.moveTo(x,y);});c.stroke();
 c.fillStyle=colors[j];c.fillRect(pad+(j%3)*Math.max(125,w/3),h-18-Math.floor(j/3)*17,9,9);c.fillStyle="#cad6e1";c.fillText(s.name,pad+14+(j%3)*Math.max(125,w/3),h-10-Math.floor(j/3)*17);});
}
function timingHeat(){if(!data.proof)return;const {c,w,h}=setupCanvas("frameheat"),p=data.proof,metric=$("metric").value;
 const all=p.policies.flatMap(s=>s.frames.map(f=>f[metric])),max=Math.max(.0001,...all),padL=115,padT=12;
 p.policies.forEach((s,j)=>{const cellH=(h-32)/p.policies.length;c.fillStyle="#acbfce";c.font="11px system-ui";c.fillText(s.name,5,padT+(j+.55)*cellH);
 s.frames.forEach((f,i)=>{let t=f[metric]/max;c.fillStyle="hsl("+Math.round(200-170*t)+",60%,"+Math.round(25+32*t)+"%)";c.fillRect(padL+i*(w-padL-12)/p.frames,padT+j*cellH,(w-padL-12)/p.frames+0.3,cellH-2);});});
 c.fillStyle="#afc4d1";c.fillText("Frame 0",padL,h-5);c.fillText("Frame "+(p.frames-1),w-87,h-5);
}
function init(){card("Material scenes",data.research?data.research.scenes.length:"Not supplied");
 card("Native compared methods",data.research?data.summary.length:"—");
 card("Solver scenes",data.research?data.research.physical.length:"Not supplied");
 card("Real cloth frames",data.proof?data.proof.frames:"Not supplied");
 buildLeaderboard();
 if(!data.research){$("stats").insertAdjacentElement("afterend",text("p","No Stage-2 research manifest supplied: algorithm heatmap and performance surface require a full native research run."));}
 if(!data.proof){$("proof-section").style.display="none";}else{
   $("metric").addEventListener("change",()=>{linePlot();timingHeat();});linePlot();timingHeat();
   const p=data.proof,ref=p.policies.find(x=>x.name==="sap_rebuild").frames;
   const crossing=ref.reduce((a,f)=>a+f.geometric_self_intersections,0);
   const contact=ref.reduce((a,f)=>a+f.self_contacts,0);
   $("correctness").textContent="Reference trajectory: "+crossing+" summed frame-end geometric intersection counts; "+contact+" same-sheet contact corrections. Pair-oracle misses and position divergence were checked during import. These are not continuous CCD or material-calibration results.";
   if(p.viewer){$("viewer").style.display="inline";$("viewer").href=p.viewer;}
 }
 let warnings=(data.research?data.research.warnings:[]).concat(["No confidence interval is computed from a single timing session.","Never infer a full-solver speedup from AABB query timings."]);
 $("limits").textContent=warnings.join("  •  ");
 window.addEventListener("resize",()=>{if(data.research)drawSurface();if(data.proof){linePlot();timingHeat();}});
}
init();
</script></body></html>'''


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--research", type=Path, help="Original Stage-2 RBC_RESULTS.json")
    p.add_argument("--proof", type=Path, help="Original NATIVE_FIVE_POLICY_SOLVER.csv")
    p.add_argument("--output", type=Path, required=True,
                   help="New output directory; original evidence is never overwritten")
    args = p.parse_args()
    if not args.research and not args.proof:
        p.error("Supply --research and/or --proof")
    if args.output.exists():
        p.error("Output already exists; choose a fresh observatory path")
    research = read_research(args.research) if args.research else None
    proof = read_proof(args.proof) if args.proof else None
    summary = aggregate(research["scenes"]) if research else []
    if research:
        table("NATIVE FULL-PAIR QUERY BENCHMARKS | Guard/method >1 means faster than Guard",
              ["METHOD", "GM SPEEDUP", "MEAN ms", "WINS", "SCENES"], [
                  [v["method"], f"{v['geomean_guard_speedup']:.4f}x",
                   f"{v['mean_query_ms']:.4f}", str(v["wins_vs_guard"]), str(v["scenes"])]
                  for v in summary
              ])
        if research["physical"]:
            table("NATIVE COUPLED WHOLE-STEP (ms) | these are NOT AABB query times",
                  ["SCENARIO", *PHYSICAL_METHODS], [
                      [x["scenario"], *(f"{x['means'][m]:.3f}" for m in PHYSICAL_METHODS)]
                      for x in research["physical"]
                  ])
    if proof:
        table("NATIVE FIVE-POLICY ORIGINAL SOLVER FRAMES | first 8 excluded",
              ["POLICY", "MEAN FULL STEP ms", "FRAMES"], [
                  [policy, f"{value:.4f}", str(proof["frames"])]
                  for policy, value in proof["mean_step"].items()
              ])
    payload = {
        "research": research, "proof": proof, "summary": summary,
        "physical_methods": PHYSICAL_METHODS,
    }
    args.output.mkdir(parents=True, exist_ok=False)
    index = args.output / "RESEARCH_OBSERVATORY.html"
    index.write_text(html_document(payload), encoding="utf-8")
    with (args.output / "LEADERBOARD.csv").open("w", newline="", encoding="utf-8") as fd:
        writer = csv.DictWriter(fd, fieldnames=["method", "geomean_guard_speedup",
                          "wins_vs_guard", "scenes", "mean_query_ms"])
        writer.writeheader()
        writer.writerows(summary)
    receipts = {
        "schema": "kyoto-rbc-read-only-research-observatory-v1",
        "research_manifest_sha256": research["sha256"] if research else None,
        "proof_native_csv_sha256": proof["sha256"] if proof else None,
        "materials": len(research["scenes"]) if research else 0,
        "physical_scenes": len(research["physical"]) if research else 0,
        "no_synthetic_data_or_rerun": True,
        "html_sha256": sha256(index),
    }
    (args.output / "OBSERVATORY_RECEIPTS.json").write_text(
        json.dumps(receipts, indent=2) + "\n", encoding="utf-8")
    print("\n[DONE] Research observatory:", index)
    print("[DONE] Auditable leaderboard:", args.output / "LEADERBOARD.csv")
    print("[NOTE] Open HTML locally; all charts and 3D performance surface are offline.")
    print("[NOTE] Original 3D cloth geometry remains in its separate native viewer.")


if __name__ == "__main__":
    main()
