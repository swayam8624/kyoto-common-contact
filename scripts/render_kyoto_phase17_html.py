#!/usr/bin/env python3
"""Render an OFFLINE, evidence-linked Kyoto method navigator.

Uses CSVs already verified by kyoto_research_dashboard.py, never makes up
timings, physical geometry or an unexecuted ML model. The classic full-pair
comparison and synthetic Phase 16 trace have separate sections.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

HTML = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kyoto — original methods and spatial pressure evidence</title>
<style>
:root{color-scheme:dark;--bg:#10151b;--panel:#19212a;--ink:#e2e9ed;--muted:#a0b0bc;--edge:#34424d;--accent:#9fd3cd}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Arial,sans-serif;color:var(--ink);background:var(--bg);line-height:1.5}
header,main,footer{max-width:1250px;margin:auto;padding:22px 26px}
header{padding-top:50px;border-bottom:1px solid var(--edge)}
small,.muted{color:var(--muted)}h1{font-size:clamp(1.8rem,4vw,2.7rem);line-height:1.12;letter-spacing:-.035em;margin:8px 0}
h2{font-size:1.35rem;margin:0 0 8px}h3{font-size:1.05rem;margin:0 0 8px}p{max-width:900px}
section{margin:28px 0 36px}.panel{border:1px solid var(--edge);border-radius:12px;background:var(--panel);padding:18px}
.controls{display:flex;gap:15px;flex-wrap:wrap;align-items:center;margin:12px 0}
label{font-size:.9rem}select,button{font:inherit;color:var(--ink);background:#222d36;border:1px solid #617280;border-radius:7px;padding:8px 11px}
button{cursor:pointer}select{max-width:100%;min-width:240px}
.legend{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:7px;margin:15px 0}
.legend label{display:flex;gap:9px;align-items:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.legend input{accent-color:#a0c4d3}.swatch{width:12px;height:12px;border-radius:3px;flex-shrink:0}
.svg-wrap{overflow:auto;border:1px solid var(--edge);border-radius:8px;background:#121a21}
svg{width:100%;min-width:650px;display:block}
table{border-collapse:collapse;font-size:.88rem;width:100%}th,td{padding:9px 11px;border-bottom:1px solid var(--edge);text-align:left}
th{color:#abc6d0;font-size:.78rem;letter-spacing:.035em;text-transform:uppercase}td.num,th.num{text-align:right;font-variant-numeric:tabular-nums}
.table-scroll{overflow:auto}code{font-size:.85em;color:#cee2db}
strong{font-weight:650}.micro{font-size:.79rem;color:var(--muted)}
.warning{padding:10px 0;border-top:1px solid var(--edge);color:var(--muted);font-size:.85rem}
footer{padding-bottom:50px;border-top:1px solid var(--edge)}a{color:#a4d9de}
</style></head><body>
<header><small>KYOTO / EXACT AABB BROAD-PHASE RESEARCH</small>
<h1>The original algorithms are still here.</h1>
<p class="muted">Rebuilt SAP · Persistent SAP · Global Guard · Selective Guard · Original Online · V4 bounded trial · Regional methods · optional learned ML and Bullet DBVT.</p>
<p>This evidence browser shows what actually executed. There are two distinct experimental families. The material replay below produces and verifies <strong>full candidate-pair lists</strong>. The million-object pressure pipeline records <strong>count and fingerprints</strong> and is deliberately separated.</p></header>
<main>
<section><h2>1. Original complete-pair material replay</h2>
<p class="muted">Every plotted algorithm uses the same recorded input trajectory. Each frame is checked against rebuilt SAP's complete pair count and hash; sampled independent brute-force checks are available where supported. These are broad-phase times, not coupled solver times.</p>
<div class="panel">
<div class="controls"><label for="scene">Trajectory</label><select id="scene"></select>
<button id="all" type="button">Show all methods</button><button id="core" type="button">Original essentials</button>
</div>
<div id="subtitle" class="micro"></div><div class="legend" id="legend"></div>
<div class="svg-wrap"><svg id="trace" viewBox="0 0 1040 350" role="img" aria-label="Measured CPU query milliseconds by frame"></svg></div>
<p id="trace-caption" class="micro"></p>
</div>
<h3 style="margin-top:22px">Across complete material trajectories</h3>
<div class="table-scroll panel"><table><thead><tr><th>Method actually executed</th><th class="num">Guard/method geomean</th><th class="num">Wins over Guard</th><th class="num">Ratio of summed scene means</th></tr></thead><tbody id="classic-table"></tbody></table></div>
<p class="micro">A missing learned model is labeled <em>not run</em>, not zero-time. A training/smoke model does not establish real ML superiority. Scores are aggregated by trajectory rather than falsely treating adjacent frames as independent subjects.</p>
</section>
<section><h2>2. Separate million-AABB pressure evidence</h2>
<p class="muted">Here the output is a count and commutative digest (not the list itself); standalone Phase 17 pair files have separate verification. Do not compare these times directly with the full-list results above.</p>
<div class="panel table-scroll"><table><thead><tr><th>Scene</th><th class="num">Objects</th><th class="num">Threads</th><th class="num">Warm full ms</th><th class="num">1-thread / current</th><th class="num">Workers</th></tr></thead><tbody id="pressure-table"></tbody></table></div>
<p class="micro">Cold-start frames are excluded from these warm means. These are synthetic AABB workloads; neither category measures narrow phase, CCD, rendering or whole physics-step time.</p></section>
<section><h2>3. Interpretation</h2><div class="panel">
<p><strong>What Phase 17 adds:</strong> a persistent-grid output callback that writes real candidate IDs into bounded, transactional binary files; a rebuilt-grid comparison; independent rehashing, duplicate checking and exact sorted pair-list equality.</p>
<p><strong>What it does not establish:</strong> that the newest spatial grid replaces V4 on original material scenes, an advantage over every optimized external library, a physical cloth-quality improvement, or a novel certified interaction-graph theorem.</p>
<p class="warning">Source evidence is reproducible from the associated CSVs and the unified JSON report. Every method in this display was actually run; absent methods were not invented. Historical negative experiments remain part of the scientific record.</p></div></section>
</main><footer><small>Generated from verified immutable local replay CSVs. No external dependencies, network requests, remote fonts or fabricated metric series.</small></footer>
<script type="application/json" id="evidence">__EVIDENCE__</script>
<script>
"use strict";
const evidence=JSON.parse(document.getElementById("evidence").textContent);
const aliases={"sap_rebuild":"Rebuilt SAP","sap_persistent_coherent":"Persistent SAP",
"global_guarded_exact":"Global Guard","selective_guard_exact":"Selective Guard",
"online_cost_aware":"Original Online","guard_sap_bounded_trial":"V4 Guard–SAP bounded trial",
"guard_sap_measured_probe":"Measured Guard–SAP probe","region_fixed":"Fixed region",
"region_adaptive":"Adaptive region","region_hierarchical":"Hierarchical region",
"ml_cost_aware":"Learned ML controller","ml_cost_aware_conservative":"Conservative learned ML",
"bullet_dbvt_persistent":"Bullet DBVT"};
const colors=["#98d4c6","#f0ca84","#b0a7e1","#86bcec","#df9ca9","#c5d999","#d4b0d4","#e2beaa","#9bc8b2","#eeab84","#f0dbb0","#7eacc6","#d6c2e8"];
const names=Object.keys(evidence.material.methods).sort((a,b)=>a.localeCompare(b));
const color=Object.fromEntries(names.map((name,i)=>[name,colors[i%colors.length]]));
const essentials=new Set(["sap_rebuild","global_guarded_exact","online_cost_aware","guard_sap_bounded_trial"]);
let selected=new Set(names.filter(x=>essentials.has(x)));
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const display=name=>aliases[name]||name;
function legend(){
 $("legend").innerHTML=names.map(name=>'<label><input type="checkbox" data-method="'+esc(name)+'" '+(selected.has(name)?"checked":"")+'>'
  +'<span class="swatch" style="background:'+color[name]+'"></span>'+esc(display(name))+'</label>').join("");
 $("legend").querySelectorAll("input").forEach(x=>x.addEventListener("change",()=>{
  x.checked?selected.add(x.dataset.method):selected.delete(x.dataset.method);draw();
 }));
}
$("all").addEventListener("click",()=>{selected=new Set(names);legend();draw();});
$("core").addEventListener("click",()=>{selected=new Set(names.filter(x=>essentials.has(x)));legend();draw();});
const trajectories=evidence.traces;
for(let i=0;i<trajectories.length;i++){
 const option=document.createElement("option");option.value=i;option.textContent=trajectories[i].name;$("scene").appendChild(option);
}
$("scene").addEventListener("change",draw);
function draw(){
 const item=trajectories[Number($("scene").value)||0];
 const active=names.filter(x=>selected.has(x)&&item.times[x]);
 const width=1040,height=350,l=74,r=30,t=18,b=48;
 const pw=width-l-r,ph=height-t-b;
 const frames=item.frame_count, ymax=Math.max(.001,...active.flatMap(x=>item.times[x]))*1.12;
 const sx=f=>l+(frames<=1?0:f/(frames-1)*pw),sy=ms=>t+ph-ms/ymax*ph;
 const svg=[];
 svg.push('<rect width="'+width+'" height="'+height+'" fill="#121a21"/>');
 for(let i=0;i<=4;i++){
  const yy=t+ph*(i/4),value=ymax*(1-i/4);
  svg.push('<line x1="'+l+'" y1="'+yy+'" x2="'+(width-r)+'" y2="'+yy+'" stroke="#34424d" stroke-width="1"/>');
  svg.push('<text x="'+(l-10)+'" y="'+(yy+4)+'" fill="#a0b0bc" font-size="12" text-anchor="end">'+value.toFixed(3)+'</text>');
 }
 for(let i=0;i<=5;i++){
  const frame=Math.round((frames-1)*i/5),xx=sx(frame);
  svg.push('<text x="'+xx+'" y="'+(height-20)+'" fill="#a0b0bc" font-size="12" text-anchor="middle">'+frame+'</text>');
 }
 for(const name of active){
  const points=item.times[name].map((ms,f)=>sx(f).toFixed(1)+','+sy(ms).toFixed(1)).join(' ');
  svg.push('<polyline fill="none" stroke="'+color[name]+'" stroke-width="2" points="'+points+'"/>');
 }
 svg.push('<text x="'+(width/2)+'" y="'+(height-5)+'" fill="#a0b0bc" text-anchor="middle" font-size="12">Frame index</text>');
 svg.push('<text x="19" y="170" fill="#a0b0bc" font-size="12" text-anchor="middle" transform="rotate(-90 19 170)">CPU query ms</text>');
 $("trace").innerHTML=svg.join("");
 $("subtitle").textContent="Actual original methods: "+names.length+" · "+frames+" validated frames · "+item.name;
 $("trace-caption").textContent=active.length? "Only selected measured methods are drawn. The vertical axis rescales to visible data, so compare values rather than apparent slopes." : "Select at least one executed method.";
}
legend();draw();
$("classic-table").innerHTML=Object.entries(evidence.material.methods)
 .sort((a,b)=>b[1].guard_speedup_geomean-a[1].guard_speedup_geomean)
 .map(([name,m])=>'<tr><td>'+esc(display(name))+' <small>('+esc(name)+')</small></td>'+
 '<td class="num">'+m.guard_speedup_geomean.toFixed(3)+'×</td>'+
 '<td class="num">'+m.trajectory_wins_vs_guard+' / '+evidence.material.trajectories+'</td>'+
 '<td class="num">'+m.summed_scene_mean_speedup_vs_guard.toFixed(3)+'×</td></tr>').join("");
const pressure=evidence.phase16;
if(pressure) {
 const rows=[];
 for(const [scene,item] of Object.entries(pressure.scenes))
  for(const [threads,m] of Object.entries(item.methods))
   rows.push('<tr><td>'+esc(scene)+'</td><td class="num">'+item.n.toLocaleString()+'</td>'+
    '<td class="num">'+threads+'</td><td class="num">'+m.warm_full_ms.toFixed(3)+'</td>'+
    '<td class="num">'+m.speedup_vs_same_scene_1thread.toFixed(3)+'×</td>'+
    '<td class="num">'+m.workers_min+'–'+m.workers_max+'</td></tr>');
 $("pressure-table").innerHTML=rows.join("");
} else {$("pressure-table").innerHTML='<tr><td colspan="6">No synthetic pressure cohort supplied.</td></tr>';}
</script></body></html>
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error(f"Refusing to overwrite existing viewer: {args.output}")
    report = json.loads(args.evidence.read_text(encoding="utf-8"))
    materials = report["material_replay"]
    traces = []
    for item in materials["files"]:
        path = Path(item["path"])
        times = defaultdict(dict)
        with path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                name, frame = row["backend"], int(row["frame"])
                if frame in times[name]:
                    raise ValueError(f"Duplicate measured method/frame {name} {frame}")
                times[name][frame] = float(row["ms"])
        count = len(times["sap_rebuild"])
        expected = set(range(count))
        if any(set(values) != expected for values in times.values()):
            raise ValueError(f"Missing frames in original evidence {path}")
        if set(times) != set(materials["methods"]):
            raise ValueError("Report method cohort and actual CSV disagree")
        traces.append({
            "name": path.stem, "frame_count": count,
            "times": {name: [values[i] for i in range(count)]
                      for name, values in sorted(times.items())},
        })
    if not traces:
        raise ValueError("No checked trajectories")
    data = {"material": materials, "phase16": report["phase16"], "traces": traces}
    payload = json.dumps(data, separators=(",", ":"), ensure_ascii=True).replace("</", "<\\/")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(HTML.replace("__EVIDENCE__", payload), encoding="utf-8")
    print(f"PASS offline research comparison: {len(traces)} real material trajectories "
          f"and {len(materials['methods'])} exact-pair algorithms, {args.output}")


if __name__ == "__main__":
    main()
