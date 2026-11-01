#!/usr/bin/env python3
"""Record actual C++ rigid-sphere snapshots into a standalone offline viewer.

This is the *independent* rigid-sphere demo for islands/analytic sphere CCD,
NOT the cloth broad-phase service or an official IPC/OGC/CCFR integration.
"""
from __future__ import annotations
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "ccp_service"
OUT = ROOT / "output" / "native" / "supporting-offline-replay.html"
if not EXE.is_file():
    raise SystemExit(f"Missing C++ service executable: {EXE}. Build the project first.")

def capture(scene: str, query: str, steps: int) -> list[dict]:
    """Command the authoritative C++ simulator; no Python physics simulation."""
    proc = subprocess.Popen([str(EXE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, bufsize=1)
    serial = 0
    def command(**kwargs):
        nonlocal serial
        serial += 1
        assert proc.stdin and proc.stdout
        proc.stdin.write(json.dumps({"id":serial, **kwargs}) + "\n")
        proc.stdin.flush()
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError(f"Core stopped before responding to {kwargs}")
        reply = json.loads(line)
        if reply.get("id") != serial or not reply.get("ok"):
            raise RuntimeError(f"C++ service rejected {kwargs}: {reply}")
        return reply["snapshot"]
    try:
        command(command="reset", scene=scene)
        command(command="broadPhase", mode="sweep")
        first = command(command="queryMode", mode=query)
        frames = [first]
        for _ in range(steps):
            frames.append(command(command="step"))
        return frames
    finally:
        if proc.stdin:
            proc.stdin.close()
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

sets = {
    "islands": capture("islands", "discrete", 120),
    "elastic": capture("elastic", "discrete", 110),
    "tunneling_discrete": capture("tunneling", "discrete", 1),
    "tunneling_swept": capture("tunneling", "swept", 1),
}
if not any(any((c["a"] < 6 <= c["b"] or c["b"] < 6 <= c["a"])
               for c in frame["contacts"]) for frame in sets["islands"]):
    raise SystemExit("Island offline replay failed: no verified cross-group contact")
if sets["tunneling_discrete"][1]["contacts"]:
    raise SystemExit("Discrete tunneling fixture unexpectedly produced a contact")
if not any(c["toi"] >= 0 for c in sets["tunneling_swept"][1]["contacts"]):
    raise SystemExit("Swept tunneling fixture did not produce a continuous impact")
if any(not frames or frames[0]["frame"] != 0 for frames in sets.values()):
    raise SystemExit("Missing initial C++ simulation state")
payload = json.dumps(sets, separators=(",", ":")).replace("</", "<\\/")

page = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CCP · Recorded C++ collision experiments</title>
<style>
:root{font-family:Inter,system-ui,-apple-system,sans-serif;color:#e8f1f9;background:#091522}
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:radial-gradient(ellipse at 35% 8%,#1c3450 0,#091522 62%)}
header{padding:25px clamp(20px,4vw,70px);border-bottom:1px solid #274056;display:flex;gap:16px;justify-content:space-between;align-items:center;flex-wrap:wrap}
h1{font-size:25px;margin:4px 0}small,.muted{color:#9caec0}label{display:grid;gap:7px;color:#abc0d3;font-size:13px}
.shell{max-width:1500px;margin:auto;padding:22px clamp(18px,4vw,60px)}
.controls{display:flex;align-items:end;flex-wrap:wrap;gap:14px;padding:15px 0}
select,button{background:#172d41;color:#f0f7ff;border:1px solid #395471;padding:11px 15px;border-radius:9px;cursor:pointer}
button:hover{background:#254564}button:focus-visible,select:focus-visible{outline:2px solid #5ce0d5}
canvas{display:block;width:100%;height:min(60vh,610px);min-height:360px;border-radius:16px;border:1px solid #38546c;background:#0d1e2e}
.timeline{display:flex;gap:15px;align-items:center;margin-top:14px}input[type=range]{width:100%;accent-color:#55daca}
.metrics{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin:18px 0}
.metric{padding:15px;background:#142a3d;border:1px solid #29445d;border-radius:12px}
.metric b{display:block;font-size:26px;margin:8px 0;color:#f2f7ff}
legend{color:#a9bbcb}.notice{max-width:950px;line-height:1.55;color:#abc0d3}
.badge{display:inline-block;color:#87ddd3;background:#16403f;padding:6px 11px;border-radius:99px;font-size:11px;font-weight:700;letter-spacing:.06em}
</style></head>
<body><header><div><span class="badge">OFFLINE · REAL C++ RECORDED SNAPSHOTS</span><h1>Common Contact Pipeline — Supporting Experiments</h1><small>Rigid spheres · candidate discovery · verified contacts · graph islands · analytic time of impact</small></div>
<small>Research meeting · Kyoto · 02 Nov 2026</small></header>
<div class="shell"><div class="controls">
<label>Experiment<select id="scene"><option value="islands">Dynamic contact islands</option><option value="elastic">Elastic collision</option><option value="tunneling_discrete">Fast crossing · endpoint only</option><option value="tunneling_swept">Fast crossing · swept + TOI</option></select></label>
<button id="play">▶ Play</button><button id="previous">← Step</button><button id="next">Step →</button>
<label><span>Candidate lines</span><input id="candidates" type="checkbox" checked></label>
<label><span>Verified contact links</span><input id="contacts" type="checkbox" checked></label>
<label><span>Color by contact island</span><input id="color" type="checkbox" checked></label>
</div><canvas id="view" aria-label="Recorded physical simulation state"></canvas>
<div class="timeline"><span id="frame">Frame 0</span><input type="range" id="slider" min="0" max="120" value="0"><span id="total">120</span></div>
<div class="metrics"><div class="metric">Bodies<b id="bodies">0</b></div><div class="metric">Candidates<b id="candidateCount">0</b></div><div class="metric">Confirmed contacts<b id="contactCount">0</b></div><div class="metric">Components<b id="islandCount">0</b></div><div class="metric">TOI (if measured)<b id="toi">—</b></div></div>
<p class="notice"><strong>Scientific boundary:</strong> These are snapshots captured from a separate C++ sphere impulse solver. The cloth experiment's XPBD-style contact pipeline is a different application. Islands are connected components of confirmed rigid-body contacts in this scene. Swept AABBs alone are not CCD; the swept sphere mode also uses an analytic time-of-impact test. No IPC, OGC or CCFR implementation is claimed.</p></div>
<script>
const runs=__SCENE_PAYLOAD__;
const $=id=>document.getElementById(id);
const cv=$("view"),ctx=cv.getContext("2d");
const palette=["#5bd7d0","#ecad62","#92b7ff","#d3a0f5","#9de2ac","#ee8d9a","#ffe08a"];
let frame=0,timer=null;
function project(p,w,h){const z=p[2]||0;const sc=Math.min(w,h)*.37;return{x:w*.5+(p[0]+z*.35)*sc,y:h*.55-(p[1]-z*.25)*sc}}
function stroke(a,b,color,width=1,dashes=[]){ctx.beginPath();ctx.setLineDash(dashes);ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();ctx.setLineDash([])}
function render(){
 const run=runs[$("scene").value],s=run[Math.min(frame,run.length-1)];
 const dpr=Math.min(2,window.devicePixelRatio||1),w=cv.clientWidth,h=cv.clientHeight;
 if(cv.width!==Math.round(w*dpr)||cv.height!==Math.round(h*dpr)){cv.width=Math.round(w*dpr);cv.height=Math.round(h*dpr)}
 ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
 for(let i=-5;i<=5;i++){let t=i/5;stroke(project([-1,t,-.2],w,h),project([1,t,-.2],w,h),"#29465b",.7);stroke(project([t,-1,-.2],w,h),project([t,1,-.2],w,h),"#29465b",.7)}
 const bodies=new Map(s.bodies.map(b=>[b.id,b]));
 if($("candidates").checked)for(const pair of s.candidatePairs){let a=bodies.get(pair[0]),b=bodies.get(pair[1]);if(a&&b)stroke(project(a.p,w,h),project(b.p,w,h),"#e5a453aa",1.3,[5,4])}
 if(s.motionPaths)for(const path of s.motionPaths){if(path.length<2)continue;for(let j=1;j<path.length;j++)stroke(project(path[j-1],w,h),project(path[j],w,h),"#64bbc9",2)}
 if($("contacts").checked)for(const c of s.contacts){let a=bodies.get(c.a),b=bodies.get(c.b);if(a&&b)stroke(project(a.p,w,h),project(b.p,w,h),"#ff7c8e",3)}
 for(const b of [...s.bodies].sort((a,b)=>a.p[2]-b.p[2])){
   const p=project(b.p,w,h),r=Math.max(7,b.r*Math.min(w,h)*.37);
   const color=$("color").checked?palette[b.island%palette.length]:"#6bb8e4";
   const g=ctx.createRadialGradient(p.x-r*.32,p.y-r*.33,1,p.x,p.y,r*1.3);
   g.addColorStop(0,"#f2fcff");g.addColorStop(.3,color);g.addColorStop(1,"#15283b");
   ctx.fillStyle=g;ctx.strokeStyle=color;ctx.lineWidth=1.7;
   ctx.beginPath();ctx.arc(p.x,p.y,r,0,Math.PI*2);ctx.fill();ctx.stroke();
   ctx.fillStyle="#071726";ctx.font="bold 12px system-ui";ctx.textAlign="center";ctx.textBaseline="middle";ctx.fillText(b.id,p.x,p.y);
 }
 $("frame").textContent="Frame "+s.frame;
 $("slider").value=frame;$("slider").max=run.length-1;$("total").textContent=run.length-1;
 $("bodies").textContent=s.bodies.length;$("candidateCount").textContent=s.candidatePairs.length;
 $("contactCount").textContent=s.contacts.length;$("islandCount").textContent=new Set(s.bodies.map(b=>b.island)).size;
 const toi=s.contacts.find(c=>c.toi>=0);$("toi").textContent=toi?(toi.toi*1000).toFixed(3)+" ms":"—";
}
function stop(){if(timer!==null){clearInterval(timer);timer=null}$("play").textContent="▶ Play"}
$("scene").onchange=()=>{stop();frame=0;render()};
$("slider").oninput=e=>{stop();frame=+e.target.value;render()};
$("play").onclick=()=>{if(timer!==null){stop();return}timer=setInterval(()=>{const run=runs[$("scene").value];if(frame>=run.length-1){stop();return}frame++;render()},70);$("play").textContent="Ⅱ Pause"};
$("previous").onclick=()=>{stop();frame=Math.max(0,frame-1);render()};
$("next").onclick=()=>{stop();frame=Math.min(runs[$("scene").value].length-1,frame+1);render()};
for(const name of ["candidates","contacts","color"])$(name).onchange=render;
window.addEventListener("resize",render);render();
</script></body></html>'''
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(page.replace("__SCENE_PAYLOAD__", payload), encoding="utf-8")
print(f"Validated offline C++ island merge and tunneling; wrote {OUT} ({OUT.stat().st_size} bytes)")
