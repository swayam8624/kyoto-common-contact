#!/usr/bin/env python3
"""Build a local, self-contained, READ-ONLY Kyoto V4/V5/V6 evidence dashboard.

Data is read from the existing complete V5/V6 RUMBLE_PATTERNS.json reports,
plus the existing final-showcase videos and metrics. No benchmarking, training,
or scientific measurements are fabricated or rerun.

From the repository root:
    python3 kyoto_dashboard.py --render-comparisons
    open output/kyoto-research-dashboard/index.html

No third-party dependencies are required for the dashboard itself. Optional
--render-comparisons invokes the repository's already-existing Pillow-based
AABB renderer ONCE per V5/V6 archived dataset, without running simulations.
"""
from __future__ import annotations

import argparse
import csv
import html
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

KNOWN = {
    "v5": {"geo": 1.091216, "wins": 667, "cells": 864, "sum": 1.328684, "aa": 1.000459},
    "v6": {"geo": 1.083201, "wins": 596, "cells": 864, "sum": 1.316721, "aa": 0.999626},
}
ALIAS = {
    "guard_sap_bounded_trial": "V4 · Guard–SAP Bounded Trial",
    "global_guarded_exact": "Global Guard (baseline)",
    "global_guarded_aa_control": "Guard A/A control",
    "sap_rebuild": "Rebuild SAP",
    "sap_persistent_coherent": "Persistent SAP",
    "unified_certified_hierarchy_v0": "V6 · Unified Certified Hierarchy",
    "bvh_refit_exact": "Refit BVH",
    "bvh_adaptive_exact": "Adaptive BVH",
    "patch_certificate_fixed": "Fixed patch certificate",
    "patch_certificate_signal": "Signal patch certificate",
    "grid_spatial_hash_exact": "Spatial hash",
    "delta_spatial_join_exact": "Incremental spatial join",
    "ml_cost_aware": "Learned cost selector",
    "ml_cost_aware_conservative": "Conservative learned selector",
}


def load_json_file(path: Path) -> dict | None:
    if not path.is_file():
        return None
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError(f"Expected JSON object: {path}")
    return data


def find_report(path: Path) -> Path | None:
    """Prefer the authoritative root report. Never select an arbitrary smoke run."""
    if path.is_file():
        return path
    p = path / "RUMBLE_PATTERNS.json"
    if p.is_file():
        return p
    p = path / "reports" / "RUMBLE_PATTERNS.json"
    if p.is_file():
        return p
    return None


def safe_float(value, label: str) -> float:
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise ValueError(f"{label}: positive finite timing or ratio required")
    return result


def validate_rumble(name: str, doc: dict, source: Path) -> dict:
    """Validate enough of the original analyzer contract to prevent fake GUI claims."""
    protocol = doc.get("protocol", {})
    cells = doc.get("cells", [])
    board = doc.get("leaderboard", [])
    if not isinstance(cells, list) or not cells:
        raise ValueError(f"{source}: no independent cells")
    if len(cells) != int(protocol.get("unique_cells", -1)):
        raise ValueError(f"{source}: unique-cell count disagrees with protocol")
    if not isinstance(board, list) or not board:
        raise ValueError(f"{source}: missing leaderboard")
    names = {x["method"] for x in board}
    if not {"global_guarded_exact", "guard_sap_bounded_trial", "sap_rebuild"}.issubset(names):
        raise ValueError(f"{source}: no mandatory V4, Guard, or SAP baseline")
    entries = []
    cohort_methods = set(names)
    for cell in cells:
        m = cell.get("mean_ms", {})
        p = cell.get("p95_ms", {})
        if set(m) != cohort_methods or set(p) != cohort_methods:
            raise ValueError(f"{source}: incomplete method roster in cell")
        for k, value in m.items():
            safe_float(value, f"{source}:{k}")
        for k, value in p.items():
            safe_float(value, f"{source}:{k} p95")
        entries.append({
            "material": cell["material"],
            "regime": cell["regime"],
            "budget": cell["budget"],
            "seed": cell["seed"],
            "n_actual": cell.get("n_actual"),
            "repeats": cell.get("repeats"),
            "mean_ms": m,
            "p95_ms": p,
            "switches": cell.get("switches", {}),
            "guard_rebuild_frequency": cell.get("guard_rebuild_frequency"),
            "fastest_hindsight_policy": cell.get("fastest_hindsight_policy"),
        })
    return {
        "name": name,
        "source": str(source),
        "protocol": {k: protocol.get(k) for k in (
            "unique_cells", "original_csv_files", "seeds", "repeats_by_cell",
            "method_order", "unit", "correctness", "limitation")},
        "leaderboard": board,
        "by_material": doc.get("by_material", {}),
        "by_regime": doc.get("by_regime", {}),
        "by_budget": doc.get("by_budget", {}),
        "guard_aa": doc.get("guard_aa", {}),
        "guard_rebuild_sap_crossover": doc.get("guard_rebuild_sap_crossover", []),
        "guard_rebuild_sap_spearman": doc.get("guard_rebuild_sap_spearman"),
        "cells": entries,
    }


def image_asset(src: Path, directory: Path, target: str) -> str | None:
    if not src.is_file():
        return None
    directory.mkdir(parents=True, exist_ok=True)
    dst = directory / target
    if dst.resolve() != src.resolve():
        shutil.copy2(src, dst)
    return f"assets/{target}"


def get_video_assets(showcase: Path, output: Path) -> dict:
    assets = {}
    for key, stem in (
        ("showcase_aabb", "all-materials-six-regimes"),
        ("showcase_solver", "all-eight-solver-policies"),
    ):
        for ext in ("gif", "mp4"):
            rel = image_asset(showcase / "videos" / f"{stem}.{ext}",
                              output / "assets", f"{stem}.{ext}")
            if rel:
                assets[f"{key}_{ext}"] = rel
    return assets


def try_render_comparison(repo: Path, cohort: Path, name: str,
                          summary: dict, output: Path) -> str | None:
    if not (cohort / "snapshots").is_dir() or not (cohort / "replays").is_dir():
        print(f"[VIDEO] {name}: raw snapshots/replays missing, skipping truthful animation")
        return None
    renderer = repo / "scripts" / "render_kyoto_visuals.py"
    if not renderer.is_file():
        print(f"[VIDEO] {name}: original renderer missing, skipping")
        return None
    have = {x["method"] for x in summary["leaderboard"]}
    methods = ["sap_rebuild", "global_guarded_exact", "guard_sap_bounded_trial",
               "patch_certificate_fixed", "bvh_refit_exact"]
    if name == "v6":
        methods.append("unified_certified_hierarchy_v0")
    methods = [x for x in methods if x in have]
    if len(methods) < 3:
        return None
    # A GIF is built only from existing same-snapshot replay CSV and binary
    # snapshots. The original renderer checks pair count/hash parity framewise.
    stem = output / "assets" / f"{name}-archived-exact-aabb"
    if stem.with_suffix(".gif").is_file():
        return f"assets/{stem.name}.gif"
    cmd = [sys.executable, str(renderer), "aabb", "--run-dir", str(cohort),
           "--output", str(stem), "--budget", "512", "--frames-per-regime", "5",
           "--fps", "6", "--methods", *methods]
    print("[VIDEO] Generating", name, "from archived evidence (no new simulation)")
    done = subprocess.run(cmd, cwd=repo, capture_output=True, text=True)
    if done.returncode != 0:
        print("[VIDEO] Skipped", name, "because the archival data or renderer was incompatible")
        print((done.stdout + "\n" + done.stderr)[-1100:])
        return None
    return f"assets/{stem.name}.gif" if stem.with_suffix(".gif").exists() else None


def json_escape_for_script(obj: object) -> str:
    return json.dumps(obj, separators=(",", ":"), ensure_ascii=False).replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")


PAGE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>KYOTO / Broad-phase research evidence</title>
<style>
:root{color-scheme:dark;--bg:#09121c;--pane:#0f1d2c;--pane2:#132335;--line:#263a50;--ink:#f0f5fa;--soft:#9db1c5;--cyan:#6cc8ee;--mint:#7bdfbc;--amber:#f1c27c;--red:#ec8e91}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 12% -5%,#16324a 0,transparent 38%),var(--bg);color:var(--ink);font:14px/1.55 system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}
.wrap{max-width:1570px;margin:auto;padding:28px 30px 90px}header{display:flex;justify-content:space-between;gap:20px;align-items:start;border-bottom:1px solid var(--line);padding-bottom:22px}
.eyebrow{font-size:11px;letter-spacing:.2em;color:var(--cyan);font-weight:700;text-transform:uppercase}h1{margin:5px 0 7px;font-size:clamp(27px,4vw,43px);letter-spacing:-.045em;line-height:1.16}h2{font-size:22px;letter-spacing:-.025em;margin:0 0 4px}h3{font-size:16px;margin:0 0 10px}p{margin:8px 0;color:var(--soft)}a{color:#a8ddff}
.pill{display:inline-block;border:1px solid #346779;border-radius:99px;padding:4px 10px;color:var(--cyan);white-space:nowrap;font-size:11px;letter-spacing:.07em}
.hero-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin:22px 0}.stat{border:1px solid var(--line);border-radius:12px;background:linear-gradient(145deg,#152b3b,#0d1a29);padding:16px 18px}.stat-label{font-size:12px;color:var(--soft)}.stat b{font-size:29px;font-variant-numeric:tabular-nums;display:block;line-height:1.3}.stat small{color:var(--soft)}
.warning{border-left:3px solid var(--amber);background:#24201b;padding:13px 16px;margin:20px 0;color:#f4dfbc}.warning strong{color:#fff0d4}
section{padding:24px 0;border-bottom:1px solid var(--line)}.section-header{display:flex;justify-content:space-between;gap:12px;align-items:end;margin-bottom:14px}.muted{color:var(--soft);font-size:12px}
.filters{display:flex;flex-wrap:wrap;gap:9px;margin:12px 0 16px}.filters label{display:flex;flex-direction:column;gap:4px;color:var(--soft);font-size:11px;text-transform:uppercase;letter-spacing:.08em}
select,input{background:#142637;color:var(--ink);border:1px solid #35516a;border-radius:6px;padding:9px 12px;min-width:135px;font:inherit}input{min-width:190px}button{background:#1c3b50;border:1px solid #345e76;color:var(--ink);padding:9px 12px;border-radius:7px;cursor:pointer}button:hover{border-color:var(--cyan)}
.two{display:grid;grid-template-columns:1fr 1fr;gap:15px}.panel{border:1px solid var(--line);background:#0d1c2b;border-radius:12px;padding:18px;min-width:0}.table-scroll{overflow:auto;max-height:660px;border:1px solid var(--line);border-radius:8px}table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums;white-space:nowrap}th,td{text-align:left;padding:10px 12px;border-bottom:1px solid #24374a;font-size:12px}th{position:sticky;top:0;background:#16293b;color:#aac3d5;font-size:11px;text-transform:uppercase;letter-spacing:.04em;z-index:1}td.num,th.num{text-align:right}tr:hover td{background:#172d40}tr.feature td:first-child{color:var(--cyan);font-weight:700}.good{color:var(--mint)}.bad{color:var(--red)}.baseline{color:#ced5df}.bar{display:inline-block;background:#1a3c4d;border-radius:2px;width:74px;height:7px;vertical-align:middle;margin-left:7px;overflow:hidden}.bar i{display:block;background:var(--mint);height:100%}.bar.negative i{background:var(--red)}
.item{display:flex;align-items:center;gap:10px;margin:10px 0}.item-label{width:172px;flex:none;font-size:12px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.item-bar{height:10px;flex:1;background:#1c3545;position:relative;max-width:400px;border-radius:4px;overflow:hidden}.item-bar i{height:100%;display:block;background:var(--cyan);border-radius:4px}.item-bar.negative i{background:var(--red)}.item-value{width:65px;text-align:right;font-variant-numeric:tabular-nums;font-size:12px}
.media{display:grid;grid-template-columns:1fr 1fr;gap:16px}.media-box{background:#101d2b;border:1px solid var(--line);border-radius:10px;padding:14px;min-width:0}.media-box img,.media-box video{width:100%;max-height:560px;object-fit:contain;background:#07111a;border-radius:6px}.media-box h3{margin-bottom:4px}.media-box p{font-size:12px}.empty{padding:34px;text-align:center;color:var(--soft);border:1px dashed #355066;border-radius:10px;background:#0c1927}
footer{font-size:12px;color:var(--soft);padding:28px 0}code{font-size:12px;color:#a6e6df;overflow-wrap:anywhere}strong{color:#f1f5f8}.badge{font-size:11px;color:var(--mint);border:1px solid #285b4a;padding:2px 6px;border-radius:5px}
@media(max-width:990px){.hero-grid{grid-template-columns:1fr 1fr}.two,.media{grid-template-columns:1fr}.wrap{padding:18px}header{flex-direction:column}}@media(max-width:540px){.hero-grid{grid-template-columns:1fr}.item-label{width:115px}}
</style></head><body><div class="wrap"><header><div><div class="eyebrow">KYOTO / Collision infrastructure / Evidence browser</div><h1>Broad-phase research<br>V4 / V5 / V6</h1><p>Auditable performance, per-material behavior, failure cases and actual motion evidence.</p></div><div class="pill">LOCAL · READ ONLY · NO BENCHMARKS RE-RUN</div></header>
<div class="hero-grid" id="hero"></div>
<div class="warning"><strong>Scientific status:</strong> V4 is a measured CPU exact-AABB candidate-generator improvement over Global Guard on these generated workloads. This is <strong>not</strong> a proven novel algorithm, a CCD implementation, a faster full physics solver or a remedy for cloth folding. V6's unified hierarchy underperformed overall. An exact-pair-equivalent broad phase should not alter solver geometry.</div>
<section><div class="section-header"><div><h2>Cross-cohort results</h2><div class="muted">Same V4 controller, separately measured V5 and independent V6 experiments. Different seeds, not frame-to-frame matched across cohorts.</div></div></div><div id="cross"></div></section>
<section><div class="section-header"><div><h2>All-method leaderboard</h2><div class="muted">Measured per-scene geometric speedup vs Global Guard; independent runs, not a hypothetical hindsight oracle.</div></div></div>
<div class="filters"><label>Experiment<select id="cohort"></select></label><label>Material<select id="material"></select></label><label>Regime<select id="regime"></select></label><label>Requested budget<select id="budget"></select></label><label>Baseline<select id="reference"></select></label></div>
<div class="two"><div class="panel"><h3>Relative speedup by method</h3><div id="rankbars"></div></div><div class="panel"><h3>Method ranking, exactness and uncertainty</h3><div class="table-scroll"><table><thead><tr><th>Method</th><th class="num">Geo ×</th><th class="num">Sum ×</th><th class="num">Wins</th><th class="num">Fastest</th></tr></thead><tbody id="leaderboard"></tbody></table></div><p class="muted">A 'fastest' count is a post-hoc ranking of independently warmed methods, not a deployable policy.</p></div></div>
</section>
<section><div class="section-header"><div><h2>Where does each algorithm win?</h2><div class="muted">Cohort-specific grouping for each material, motion regime and requested primitive budget.</div></div></div><div class="two"><div class="panel"><h3>Material breakdown</h3><div id="byMaterial"></div></div><div class="panel"><h3>Motion-regime breakdown</h3><div id="byRegime"></div></div></div><div class="panel" style="margin-top:14px"><h3>Size-target breakdown</h3><div id="byBudget"></div></div></section>
<section><div class="section-header"><div><h2>Every measured scenario</h2><div class="muted">Filter individual material × regime × budget × seed cells. Measured per-policy mean and P95 from the archived reports.</div></div></div>
<div class="filters"><label>Search seed / material<input id="search" placeholder="cloth, 3203, rapid..."></label><label>Order<select id="order"><option value="speed">V4 speedup: best first</option><option value="slow">V4 speedup: worst first</option><option value="name">Material / regime / seed</option></select></label><label>Show method<select id="sceneMethod"></select></label></div><div class="muted" id="sceneCount"></div>
<div class="table-scroll"><table><thead><tr><th>Material</th><th>Regime</th><th class="num">Budget</th><th class="num">Actual AABBs</th><th class="num">Seed</th><th class="num">Guard ms</th><th class="num">Selected ms</th><th class="num">P95 ms</th><th class="num">Speedup</th><th>Hindsight fastest</th></tr></thead><tbody id="scenes"></tbody></table></div></section>
<section><div class="section-header"><div><h2>Actual motion / visual evidence</h2><div class="muted">Archived measured AABBs and coupled solver meshes—not a legacy live simulator.</div></div></div><div class="media" id="media"></div></section>
<section><div class="section-header"><div><h2>Correctness, failure modes and provenance</h2></div></div><div class="two"><div class="panel"><h3>What was established</h3><p>Every measured method in the V5/V6 analyzer had to match SAP's full per-frame discrete AABB pair counts and pair hashes, with a sampled independent brute-force oracle for small scenes.</p><p>The eight-policy coupled-solver visual suite agreed with the SAP trajectory to within 10⁻⁷ in the tested run. That is parity, <strong>not</strong> superior cloth physics.</p><p>Exactness of sampled AABBs does not prove continuous collision detection, nonpenetration or physical accuracy.</p></div><div class="panel"><h3>What still blocks a paper</h3><p><strong>Novelty:</strong> the V4 switching mechanism must be positioned against existing adaptive broad-phase and temporal-coherence techniques; an improvement over one Guard baseline is insufficient.</p><p><strong>Competitive baselines:</strong> compare against tuned persistent SAP, production-quality BVH/LBVH, patch methods and stronger deployable online policies.</p><p><strong>Real impact:</strong> measure complete solver-step speed, contact/narrow-phase overhead, P95/memory, and independent realistic cloth/soft-body inputs. Current replay results are not solver speedup evidence.</p></div></div><p class="muted" id="provenance"></p></section>
<footer>Offline scientific evidence viewer. No network, model fitting, simulation, background service or benchmark recomputation. All results are loaded from archival JSON, not estimated by this page.</footer>
</div><script id="evidence" type="application/json">__DATA__</script><script>
const D=JSON.parse(document.getElementById('evidence').textContent),meta=D.alias||{},cohorts=D.cohorts||{},keys=Object.keys(cohorts);
const $=id=>document.getElementById(id),name=x=>meta[x]||x, fmt=(v,n=3)=>Number.isFinite(+v)?(+v).toFixed(n):'—',xformat=v=>fmt(v,3)+'×';
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function opt(sel,values,all=false){let old=sel.value;sel.innerHTML=(all?'<option value="all">All</option>':'')+values.map(v=>`<option value="${esc(v)}">${esc(v)}</option>`).join('');sel.value=values.includes(old)?old:(all?'all':values[0])}
function entry(c,key){return (c.leaderboard||[]).find(x=>x.method===key)}
function bootstrap(e){let ci=e?.seed_block_bootstrap_95pct;return ci&&ci.length===2?`<span class="muted">[${fmt(ci[0])}, ${fmt(ci[1])}]</span>`:''}
function cls(x){return x>1.005?'good':x<.995?'bad':'baseline'}
function stat(l,v,sub){return `<div class="stat"><div class="stat-label">${l}</div><b>${v}</b><small>${sub}</small></div>`}
function renderHero(){let a=cohorts.v5,b=cohorts.v6;let e=a&&entry(a,'guard_sap_bounded_trial'),f=b&&entry(b,'guard_sap_bounded_trial');$('hero').innerHTML=stat('V5 · V4/Guard geo',e?xformat(e.geometric_speedup_vs_guard):'1.091×',a?(a.cells.length+' unique workload cells'):'Historical 864-cell report')+stat('V6 · V4/Guard geo',f?xformat(f.geometric_speedup_vs_guard):'1.083×','Independent confirmation cohort')+stat('V5 · scene wins',e?`${e.mean_scene_wins_vs_guard}/${a.cells.length}`:'667/864','Against Global Guard')+stat('V6 · scene wins',f?`${f.mean_scene_wins_vs_guard}/${b.cells.length}`:'596/864','Against Global Guard')}
function renderCross(){let rows=['v5','v6'].filter(k=>cohorts[k]).map(k=>{let c=cohorts[k],e=entry(c,'guard_sap_bounded_trial'),failed=entry(c,'unified_certified_hierarchy_v0'),aa=entry(c,'global_guarded_aa_control');return `<tr><td><strong>${k.toUpperCase()}</strong></td><td class="num">${c.cells.length}</td><td class="num ${cls(e.geometric_speedup_vs_guard)}">${xformat(e.geometric_speedup_vs_guard)}</td><td class="num">${xformat(e.sum_time_speedup_vs_guard)}</td><td class="num">${e.mean_scene_wins_vs_guard}/${c.cells.length}</td><td class="num">${xformat(aa?.geometric_speedup_vs_guard)}</td><td class="num ${failed?cls(failed.geometric_speedup_vs_guard):''}">${failed?xformat(failed.geometric_speedup_vs_guard):'Not tested'}</td></tr>`});$('cross').innerHTML=rows.length?`<div class="table-scroll"><table><thead><tr><th>Dataset</th><th class="num">Unique cells</th><th class="num">V4 geometric</th><th class="num">V4 summed-time</th><th class="num">V4 wins</th><th class="num">Guard A/A</th><th class="num">V6 hierarchy</th></tr></thead><tbody>${rows.join('')}</tbody></table></div>`:'<div class="empty">No RUMBLE_PATTERNS.json reports loaded. Supply archived V5 and V6 folders.</div>'}
function selected(){return cohorts[$('cohort').value]}
function validCells(){let c=selected();if(!c)return[];return c.cells.filter(x=>($('material').value==='all'||x.material===$('material').value)&&($('regime').value==='all'||x.regime===$('regime').value)&&($('budget').value==='all'||String(x.budget)===$('budget').value))}
function summarize(cells,ref){let methods=Object.keys(cells[0]?.mean_ms||{});return methods.map(method=>{let n=cells.length,r=cells.map(x=>x.mean_ms[ref]/x.mean_ms[method]);let pg=cells.map(x=>x.p95_ms[ref]/x.p95_ms[method]);return{method,geometric_speedup_vs_guard:Math.exp(r.reduce((a,x)=>a+Math.log(x),0)/n),sum_time_speedup_vs_guard:cells.reduce((a,x)=>a+x.mean_ms[ref],0)/cells.reduce((a,x)=>a+x.mean_ms[method],0),p95_geometric_speedup_vs_guard:Math.exp(pg.reduce((a,x)=>a+Math.log(x),0)/n),mean_scene_wins_vs_guard:r.filter(x=>x>1).length,fastest_scene_count:cells.filter(x=>Object.keys(x.mean_ms).reduce((a,b)=>x.mean_ms[a]<=x.mean_ms[b]?a:b)===method).length}}).sort((a,b)=>b.geometric_speedup_vs_guard-a.geometric_speedup_vs_guard)}
function renderRanking(){let c=selected(),cells=validCells(),ref=$('reference').value;if(!cells.length){$('rankbars').innerHTML='<div class="empty">No cells match.</div>';$('leaderboard').innerHTML='';return}let b=summarize(cells,ref);let max=Math.max(1.1,...b.map(x=>x.geometric_speedup_vs_guard));$('rankbars').innerHTML=b.map(x=>{let val=x.geometric_speedup_vs_guard;return `<div class="item"><span class="item-label" title="${esc(name(x.method))}">${esc(name(x.method))}</span><span class="item-bar ${val<1?'negative':''}"><i style="width:${Math.max(2,Math.min(100,val/max*100))}%"></i></span><span class="item-value ${cls(val)}">${xformat(val)}</span></div>`}).join('');let original=$('material').value==='all'&&$('regime').value==='all'&&$('budget').value==='all'&&ref==='global_guarded_exact';$('leaderboard').innerHTML=b.map(x=>{let prior=original?entry(c,x.method):null;return `<tr class="${x.method==='guard_sap_bounded_trial'?'feature':''}"><td>${esc(name(x.method))} ${prior?bootstrap(prior):''}</td><td class="num ${cls(x.geometric_speedup_vs_guard)}">${xformat(x.geometric_speedup_vs_guard)}</td><td class="num">${xformat(x.sum_time_speedup_vs_guard)}</td><td class="num">${x.mean_scene_wins_vs_guard}/${cells.length}</td><td class="num">${x.fastest_scene_count}</td></tr>`}).join('')}
function grouped(cells,key,method,ref){let groups={};for(let x of cells)(groups[String(x[key])]??=[]).push(x);return Object.entries(groups).sort(([a],[b])=>a.localeCompare(b,undefined,{numeric:true})).map(([title,rows])=>{let ratios=rows.map(x=>x.mean_ms[ref]/x.mean_ms[method]);return{title,n:rows.length,geo:Math.exp(ratios.reduce((a,b)=>a+Math.log(b),0)/ratios.length)}})}
function renderGroups(){let c=selected(),cells=validCells(),method=$('sceneMethod').value,ref=$('reference').value;if(!cells.length)return;for(let [id,key] of [['byMaterial','material'],['byRegime','regime'],['byBudget','budget']]){let list=grouped(cells,key,method,ref),mx=Math.max(1.2,...list.map(x=>x.geo));$(id).innerHTML=`<div class="muted">Selected: ${esc(name(method))} vs ${esc(name(ref))}</div>`+list.map(v=>`<div class="item"><span class="item-label">${esc(v.title)} <span class="muted">(${v.n})</span></span><span class="item-bar ${v.geo<1?'negative':''}"><i style="width:${Math.max(3,Math.min(100,100*v.geo/mx))}%"></i></span><span class="item-value ${cls(v.geo)}">${xformat(v.geo)}</span></div>`).join('')}}
function renderScenes(){let c=selected();if(!c)return;let method=$('sceneMethod').value,ref=$('reference').value,term=$('search').value.trim().toLowerCase();let rows=validCells().filter(x=>`${x.material} ${x.regime} ${x.budget} ${x.seed}`.toLowerCase().includes(term)),sort=$('order').value;let ratio=x=>x.mean_ms[ref]/x.mean_ms[method];rows.sort((a,b)=>sort==='name'?`${a.material}/${a.regime}/${a.seed}`.localeCompare(`${b.material}/${b.regime}/${b.seed}`):sort==='speed'?ratio(b)-ratio(a):ratio(a)-ratio(b));$('sceneCount').textContent=`${rows.length} complete independent cells; showing first ${Math.min(rows.length,750)}. All methods verified against the frozen AABB pair oracle by original analyzer.`;$('scenes').innerHTML=rows.slice(0,750).map(x=>{let s=ratio(x);return `<tr><td>${esc(x.material)}</td><td>${esc(x.regime)}</td><td class="num">${x.budget}</td><td class="num">${x.n_actual??'—'}</td><td class="num">${x.seed}</td><td class="num">${fmt(x.mean_ms[ref],5)}</td><td class="num">${fmt(x.mean_ms[method],5)}</td><td class="num">${fmt(x.p95_ms[method],5)}</td><td class="num ${cls(s)}">${xformat(s)}</td><td>${esc(name(x.fastest_hindsight_policy))}</td></tr>`}).join('')}
function mediaBlock(title,desc,stem){let gif=D.assets[stem+'_gif'],mp4=D.assets[stem+'_mp4'];let media=gif?`<img loading="lazy" src="${esc(gif)}" alt="${esc(title)}">`:mp4?`<video controls loop muted autoplay playsinline src="${esc(mp4)}"></video>`:`<div class="empty">No archived GIF loaded. Original data is preserved; use --render-comparisons for V5/V6, or point to existing showcase.</div>`;return `<div class="media-box"><h3>${esc(title)}</h3><p>${esc(desc)}</p>${media}${mp4?`<p><a href="${esc(mp4)}">Open MP4</a></p>`:''}</div>`}
function renderMedia(){$('media').innerHTML=mediaBlock('V5 · material × algorithm replay','Actual frozen V5 AABBs and measured method latencies, not synthetic animation.','v5_replay')+mediaBlock('V6 · hierarchy and alternatives','Archived V6 snapshots and CPU timings; different seed cohort from V5.','v6_replay')+mediaBlock('Six materials × six deformation regimes','Existing 36-case showcase with four algorithms, rotating through motion regimes.','showcase_aabb')+mediaBlock('Eight-policy coupled cloth and spheres','Actual mesh trajectories, including V4. Exact policies should match physics.','showcase_solver')}
function renderProvenance(){let c=selected();$('provenance').innerHTML=c?`<strong>Current source:</strong> <code>${esc(c.source)}</code> · ${c.protocol.unique_cells} independent cells · ${c.protocol.original_csv_files||'?'} replay files · seeds ${esc((c.protocol.seeds||[]).join(', '))}. Repeated runs are combined by median; uncertainty estimates resample seed blocks.`:'No original tournament reports were found. Overview information alone is not enough for per-method performance.'}
function refresh(){renderRanking();renderGroups();renderScenes();renderProvenance()}
if(keys.length){opt($('cohort'),keys);let c=selected();opt($('material'),[...new Set(c.cells.map(x=>x.material))].sort(),true);opt($('regime'),[...new Set(c.cells.map(x=>x.regime))].sort(),true);opt($('budget'),[...new Set(c.cells.map(x=>String(x.budget)))].sort((a,b)=>+a-+b),true);opt($('reference'),Object.keys(c.cells[0].mean_ms));$('reference').value='global_guarded_exact';opt($('sceneMethod'),Object.keys(c.cells[0].mean_ms));$('sceneMethod').value='guard_sap_bounded_trial';$('cohort').addEventListener('change',()=>{let c=selected();opt($('material'),[...new Set(c.cells.map(x=>x.material))].sort(),true);opt($('regime'),[...new Set(c.cells.map(x=>x.regime))].sort(),true);opt($('budget'),[...new Set(c.cells.map(x=>String(x.budget)))].sort((a,b)=>+a-+b),true);opt($('reference'),Object.keys(c.cells[0].mean_ms));opt($('sceneMethod'),Object.keys(c.cells[0].mean_ms));if(!Object.keys(c.cells[0].mean_ms).includes($('reference').value))$('reference').value='global_guarded_exact';refresh()});for(let id of ['material','regime','budget','reference','sceneMethod','search','order'])$(id).addEventListener(id==='search'?'input':'change',refresh);refresh()}else{$('cohort').innerHTML='<option>Data missing</option>';$('rankbars').innerHTML='<div class="empty">Place verified RUMBLE_PATTERNS.json inside the V5/V6 archived directories.</div>';}
renderHero();renderCross();renderMedia();
</script></body></html>'''



def write_tables(cohorts: dict, output: Path) -> None:
    """Produce machine-readable and paper-reviewable tables from unchanged data."""
    lines = [
        "# Kyoto — measured broad-phase evidence", "",
        "All speedups are CPU exact AABB candidate-query times on frozen replay",
        "trajectories. They are **not** full physics-step, GPU, CCD, contact",
        "quality, or publication-novelty claims.", "",
    ]
    leaderboard_csv = output / "LEADERBOARD_ALL_METHODS.csv"
    scenes_csv = output / "ALL_SCENES_ALL_METHODS.csv"
    with leaderboard_csv.open("w", newline="", encoding="utf-8") as lf, \
         scenes_csv.open("w", newline="", encoding="utf-8") as sf:
        lw = csv.writer(lf)
        sw = csv.writer(sf)
        lw.writerow(("cohort", "method", "geo_speedup_vs_guard", "sum_time_speedup_vs_guard",
                     "p95_geo_speedup_vs_guard", "wins_vs_guard", "fastest_scene_count",
                     "seed_block_ci_low", "seed_block_ci_high"))
        sw.writerow(("cohort", "material", "regime", "requested_budget", "actual_aabbs",
                     "seed", "repeats", "method", "mean_ms", "p95_ms",
                     "speedup_vs_guard_mean", "hindsight_fastest_method"))
        for name, doc in cohorts.items():
            cells = doc["cells"]
            board = doc["leaderboard"]
            lines.extend((f"## {name.upper()} — {len(cells)} independent scenes", "",
                          "| Algorithm | Geo × vs Guard | Summed-time × | P95 × | Wins | Fastest scenes | Seed-block 95% CI |",
                          "|---|---:|---:|---:|---:|---:|---|"))
            for item in board:
                ci = item.get("seed_block_bootstrap_95pct")
                low = ci[0] if ci and len(ci) == 2 else ""
                high = ci[1] if ci and len(ci) == 2 else ""
                method = item["method"]
                lw.writerow((name, method, item["geometric_speedup_vs_guard"],
                             item["sum_time_speedup_vs_guard"],
                             item.get("p95_geometric_speedup_vs_guard", ""),
                             item["mean_scene_wins_vs_guard"],
                             item["fastest_scene_count"], low, high))
                ci_text = f"[{low:.3f}, {high:.3f}]" if low != "" else "—"
                lines.append(f"| {method} | {item['geometric_speedup_vs_guard']:.4f}× | "
                             f"{item['sum_time_speedup_vs_guard']:.4f}× | "
                             f"{(format(item['p95_geometric_speedup_vs_guard'], '.4f') + '×') if item.get('p95_geometric_speedup_vs_guard') is not None else '—'} | "
                             f"{item['mean_scene_wins_vs_guard']}/{len(cells)} | "
                             f"{item['fastest_scene_count']} | {ci_text} |")
            lines.extend(("", f"**Source:** `{doc['source']}`", "",
                          "### V4 by material / motion regime / budget", ""))
            v4 = "guard_sap_bounded_trial"
            for label, field in (("Material", "by_material"),
                                 ("Motion regime", "by_regime"),
                                 ("Requested budget", "by_budget")):
                lines.extend((f"**{label}**", "",
                              "| Group | Scenes | V4 geo × vs Guard | V4 wins | Lowest summed-time method |",
                              "|---|---:|---:|---:|---|"))
                for group, item in doc.get(field, {}).items():
                    perf = item.get("methods", {}).get(v4, {})
                    if not perf:
                        continue
                    lines.append(f"| {group} | {item['cell_count']} | "
                                 f"{perf['geometric_speedup_vs_guard']:.4f}× | "
                                 f"{perf['scene_wins_vs_guard']} | "
                                 f"{item['lowest_summed_scene_time_policy']} |")
                lines.append("")
            for cell in cells:
                guard = cell["mean_ms"]["global_guarded_exact"]
                for method, ms in cell["mean_ms"].items():
                    sw.writerow((name, cell["material"], cell["regime"], cell["budget"],
                                 cell["n_actual"], cell["seed"], cell["repeats"],
                                 method, ms, cell["p95_ms"][method], guard/ms,
                                 cell["fastest_hindsight_policy"]))
    lines.extend((
        "## Interpretation", "",
        "- The V4 method is evaluated *within* V5 and V6. V5 and V6 are experiment",
        "  cohorts, not interchangeable versions of the same controller.",
        "- Geometric speedup gives each unique material × regime × budget × seed",
        "  cell one equal-weight vote in log space. Summed-time speedup weights",
        "  expensive scenes more heavily.",
        "- A/A Guard near 1× supports timing fairness but is not a universal",
        "  guarantee; all comparisons are still CPU and generator-dependent.",
        "- The hindsight fastest method is a retrospective diagnostic, not",
        "  an online deployable decision rule.",
        "- Solver-geometry parity and exact per-frame AABB pairs do not show",
        "  fewer folds or physically accurate continuous contact.",
        "- Method novelty requires a dedicated prior-art comparison and new",
        "  full-solver/external workload evidence.",
    ))
    (output / "RESEARCH_TABLES.md").write_text("\n".join(lines)+"\n", encoding="utf-8")

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    home = Path.home()
    p.add_argument("--repo", type=Path, default=Path.cwd(), help="Canonical Kyoto repository")
    p.add_argument("--v5", type=Path, default=home / "Documents/KyotoResearchEvidence/v5")
    p.add_argument("--v6", type=Path, default=home / "Documents/KyotoResearchEvidence/v6")
    p.add_argument("--showcase", type=Path, help="Existing visual showcase directory")
    p.add_argument("--output", type=Path, help="New output directory; safe to replace generated dashboard")
    p.add_argument("--render-comparisons", action="store_true", help="Render archival V5/V6 snapshots with existing renderer (no physics rerun)")
    a = p.parse_args()
    repo = a.repo.expanduser().resolve()
    if not (repo / "CMakeLists.txt").is_file():
        p.error(f"Not a Kyoto repository root: {repo}; pass --repo")
    output = (a.output or repo / "output/kyoto-research-dashboard").expanduser().resolve()
    if output == repo or output == Path.home() or output == Path("/"):
        p.error("Refusing dangerous output destination")
    output.mkdir(parents=True, exist_ok=True)
    assets = get_video_assets((a.showcase or repo / "output/kyoto-final-showcase").expanduser().resolve(),output)
    data = {}
    for name, raw in (("v5", a.v5), ("v6", a.v6)):
        path = raw.expanduser().resolve()
        report = find_report(path)
        if report is None:
            print(f"[MISSING] {name.upper()}: RUMBLE_PATTERNS.json under {path}")
            continue
        summary = validate_rumble(name, load_json_file(report), report)
        data[name] = summary
        v4 = next(x for x in summary["leaderboard"] if x["method"] == "guard_sap_bounded_trial")
        print(f"[LOADED] {name.upper()}: {len(summary['cells'])} scenes, V4 geo "+
              f"{v4['geometric_speedup_vs_guard']:.6f}x; {v4['mean_scene_wins_vs_guard']} wins")
        expected = KNOWN[name]
        if len(summary["cells"]) == expected["cells"]:
            if abs(float(v4["geometric_speedup_vs_guard"]) - expected["geo"]) > .02:
                print(f"[WARN] {name.upper()} does NOT match historical frozen outcome. Check provenance.")
        if a.render_comparisons:
            visual = try_render_comparison(repo, path if path.is_dir() else path.parent, name, summary, output)
            if visual:
                assets[f"{name}_replay_gif"] = visual
    write_tables(data, output)
    payload = {"cohorts": data, "assets": assets, "alias": ALIAS}
    page = PAGE.replace("__DATA__", json_escape_for_script(payload))
    index = output / "index.html"
    index.write_text(page, encoding="utf-8")
    print("[DONE] Dashboard:", index)
    print("[DONE] Full tables:", output / "RESEARCH_TABLES.md")
    print("[DONE] Exported CSVs: LEADERBOARD_ALL_METHODS.csv, ALL_SCENES_ALL_METHODS.csv")
    if not data:
        print("[WARNING] No complete V5/V6 reports found: tables need archived RUMBLE_PATTERNS.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())