#!/usr/bin/env python3
"""Render truthful, automatically looping Kyoto comparison GIFs/MP4s.

Subcommands:
  solver: REAL meshes from ccp_visual_multicloth_export. Every broad-phase
          policy must reproduce the same solver geometry within 1e-7.
          Plots area collapse/strain/contact responses, NOT a fabricated
          visual improvement from identical exact broad-phase pair sets.
  aabb:   SAME frozen boxes repeated in separate method cells, annotated
          with each method's own measured full-query latency and candidate
          counts from its real replay CSV; six materials and six regimes.

Requires: Pillow (python3 -m pip install Pillow). Optional: ffmpeg for MP4.
All outputs are NEW files in the requested directory; no input is modified.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import shutil
import struct
import subprocess
import tempfile
from collections import defaultdict
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    raise SystemExit("Pillow is required: python3 -m pip install Pillow") from exc

MATERIALS=("cloth","sponge","cushion","soft_ball","spring","mixed")
REGIMES=("baseline","compression","shear","impact","rapid","relax")
DEFAULT_METHODS=("sap_rebuild","global_guarded_exact","sap_persistent_coherent","guard_sap_bounded_trial")
COLORS=("#65c5ff","#ffb86e","#e2a7ff","#89dab3","#f9e678","#ffa1ac")
DARK="#0d1422"
WHITE="#eaf0fb"
SUB="#a6b6ca"

def font(size):
    for candidate in (
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
    ):
        try:return ImageFont.truetype(candidate,size=size)
        except OSError:pass
    return ImageFont.load_default()

FT=font(16)
FS=font(12)
FH=font(26)

def rounded(draw,xy,fill=DARK,outline="#263951",radius=9):
    draw.rounded_rectangle(xy,radius=radius,fill=fill,outline=outline,width=1)

def project(point):
    x,y,z=point
    # Fixed, orthographic oblique camera: same for all algorithms and frames.
    return (x-.61*y,z+.38*(x+y))

def bounds2d(points):
    projected=[project(p) for p in points]
    return (min(p[0] for p in projected),max(p[0] for p in projected),
            min(p[1] for p in projected),max(p[1] for p in projected))

def mapper(world,panel):
    mnx,mxx,mny,mxy=world
    x0,y0,x1,y1=panel
    w=max(1.,mxx-mnx);h=max(1.,mxy-mny)
    scale=min((x1-x0)/w,(y1-y0)/h)*.90
    midx=(mnx+mxx)/2.;midy=(mny+mxy)/2.
    mx=(x0+x1)/2.;my=(y0+y1)/2.
    def convert(p):
        a,b=project(p)
        return (int(mx+(a-midx)*scale),int(my-(b-midy)*scale))
    return convert

def annotate(draw,x,y,rows,color=WHITE):
    for j,row in enumerate(rows):
        draw.text((x,y+j*17),str(row),font=FS,fill=color)

def encode(frames,output,fps):
    output=Path(output)
    output.parent.mkdir(parents=True,exist_ok=True)
    if not frames:raise ValueError("No animation frames")
    gif=output.with_suffix(".gif")
    # Palette conversion bounds memory substantially on long montage runs.
    palette=[frame.convert("P",palette=Image.Palette.ADAPTIVE,colors=128)
             for frame in frames]
    palette[0].save(gif,save_all=True,append_images=palette[1:],
                    duration=max(40,round(1000/fps)),loop=0,
                    optimize=False,disposal=2)
    mp4=None
    if shutil.which("ffmpeg"):
        mp4=output.with_suffix(".mp4")
        with tempfile.TemporaryDirectory(prefix="kyoto-loop-frames-") as tmp:
            for i,frame in enumerate(frames):
                frame.save(Path(tmp)/f"{i:04d}.png")
            subprocess.run([
                "ffmpeg","-hide_banner","-loglevel","error","-y",
                "-framerate",str(fps),"-i",str(Path(tmp)/"%04d.png"),
                "-vf","pad=ceil(iw/2)*2:ceil(ih/2)*2",
                "-c:v","libx264","-pix_fmt","yuv420p","-crf","20",
                "-movflags","+faststart",str(mp4),
            ],check=True)
    return gif,mp4

def read_solver(directory):
    streams={}
    for path in sorted(Path(directory).glob("*.jsonl")):
        lines=(json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()
               if line.strip())
        header=next(lines)
        if header.get("type")!="mesh_header":raise ValueError(f"{path}: invalid mesh header")
        frames=list(lines)
        if not frames or any(f.get("type")!="frame" for f in frames):
            raise ValueError(f"{path}: missing frames")
        policy=header["policy"]
        if policy in streams:raise ValueError("duplicate solver policy")
        streams[policy]=(header,frames)
    if not streams:raise ValueError(f"No .jsonl meshes in {directory}")
    total={len(v[1]) for v in streams.values()}
    if len(total)!=1:raise ValueError("inconsistent solver frame counts")
    # Exact broad-phase policies must reproduce identical solver geometry.
    reference=streams.get("sap_rebuild")
    if reference:
        positions=[f["positions"] for f in reference[1]]
        for method,(header,frames) in streams.items():
            if len(header["triangles"])!=len(reference[0]["triangles"]):
                raise ValueError("mesh topology changed between solvers")
            for i,frame in enumerate(frames):
                p=frame["positions"]
                if len(p)!=len(positions[i]):
                    raise ValueError("vertex count changed")
                worst=max((max(abs(a[k]-b[k]) for k in range(3))
                           for a,b in zip(p,positions[i])),default=0.)
                if worst>1.e-7:
                    raise ValueError(f"solver physics differs for {method}, frame {i}, max {worst}")
    return streams

def render_solver(directory,out,fps=9,stride=2,frame_limit=0):
    streams=read_solver(directory)
    policies=list(streams)
    total=len(next(iter(streams.values()))[1])
    sample=list(range(0,total,max(1,stride)))
    if frame_limit:sample=sample[:frame_limit]
    # Global stable camera: no policy gets zoomed, rotated or retimed.
    allpoints=[p for header,frames in streams.values()
               for frame in frames for p in frame["positions"]]
    world=bounds2d(allpoints)
    cols=4;rows=math.ceil(len(policies)/cols)
    pw=330;ph=275
    w=pw*cols;h=86+ph*rows+50
    result=[]
    for index in sample:
        canvas=Image.new("RGB",(w,h),DARK);draw=ImageDraw.Draw(canvas)
        draw.text((24,14),"KYOTO  |  COUPLED PHYSICS, EXACT BROAD-PHASE COMPARISON",
                  font=FH,fill=WHITE)
        draw.text((24,55),f"Simulation frame {index+1}/{total}. Identical geometry expected across correct policies; render differences are failures.",
                  font=FS,fill=SUB)
        for j,method in enumerate(policies):
            col=j%cols;row=j//cols
            x=col*pw;y=84+row*ph
            rounded(draw,(x+6,y+3,x+pw-6,y+ph-8))
            draw.text((x+18,y+12),method.replace("_"," "),fill=COLORS[j%len(COLORS)],font=FT)
            header,seq=streams[method];fr=seq[index]
            positions=fr["positions"];triangles=header["triangles"]
            convert=mapper(world,(x+22,y+64,x+pw-22,y+ph-74))
            # Painter's ordering for a consistent, depth-cued filled mesh.
            order=sorted(range(len(triangles)),
                         key=lambda i:sum(positions[k][1] for k in triangles[i])/3.)
            for ti in order:
                t=triangles[ti]
                polygon=[convert(positions[k]) for k in t]
                sheet=header["triangle_sheets"][ti]
                fill=COLORS[sheet%len(COLORS)]
                # Draw mesh edges for visible wrinkling/fold motion.
                draw.polygon(polygon,fill=fill)
                draw.line(polygon+[polygon[0]],fill="#253a54",width=1)
            for sphere in fr.get("spheres",[]):
                cx,cy=convert(sphere["center"]);radius=5
                draw.ellipse((cx-radius,cy-radius,cx+radius,cy+radius),
                             fill="#ffe38b",outline="#fff4d1",width=1)
            m=fr["metrics"]
            annotate(draw,x+18,y+ph-70,[
                f"Step {m['step_ms']:.2f} ms  |  Broad {m['broad_ms']:.2f} ms",
                f"Edge strain {m['rms_relative_edge_strain']:.3f}  |  Collapsed {m['near_degenerate_triangles']}",
                f"Self-crossing responses {m['self_crossing_responses']}  |  SAP error {m['max_position_difference_from_SAP']:.1g}"
            ])
        draw.text((24,h-30),"Quality metrics are diagnostics, not a full intersection-free / CCD proof. Video repeats automatically.",
                  font=FS,fill=SUB)
        result.append(canvas)
    return encode(result,out,fps)

def snapshot_header(path):
    with Path(path).open("rb") as fd:
        head=fd.read(24)
    if len(head)!=24:raise ValueError("Truncated KYOTOM01")
    magic,n,frames,seed,material=struct.unpack("<8sIIII",head)
    if magic!=b"KYOTOM01" or n==0 or n>50000 or frames==0 or frames>10000:
        raise ValueError(f"{path}: invalid snapshot")
    if Path(path).stat().st_size!=24+frames*n*24:
        raise ValueError(f"{path}: snapshot length mismatch")
    return n,frames,seed,material

def snapshot_frame(path,frame,max_boxes=55):
    n,frames,seed,material=snapshot_header(path)
    if not 0<=frame<frames:raise ValueError("frame outside snapshot")
    # Visualization samples uniformly; benchmark correctness uses ALL boxes.
    picks=[min(n-1,round(i*(n-1)/(min(n,max_boxes)-1)))
           for i in range(min(n,max_boxes))] if n>1 else [0]
    with Path(path).open("rb") as fd:
        out=[]
        for k in picks:
            fd.seek(24+frame*n*24+k*24)
            a=struct.unpack("<6f",fd.read(24))
            out.append(a)
    return out,n

def scene_evidence(run_dir,material,regime,budget,seed,repeat,methods):
    base=f"actual-{material}-{regime}-n{budget}-seed{seed}"
    snap=Path(run_dir)/"snapshots"/f"{base}.kmcb"
    replay=Path(run_dir)/"replays"/f"{base}-rep{repeat}-evaluated.csv"
    if not replay.is_file():
        replay=Path(run_dir)/"replays"/f"{base}-evaluated.csv"
    if not snap.is_file() or not replay.is_file():
        return None
    measures=defaultdict(dict)
    with replay.open(newline="",encoding="utf-8") as fd:
        reader=csv.DictReader(fd)
        for r in reader:
            if r["backend"] in methods:
                frame=int(r["frame"])
                if frame in measures[r["backend"]]:
                    raise ValueError("Duplicate backend/frame")
                measures[r["backend"]][frame]={
                    "ms":float(r["ms"]),"pairs":int(r["pairs"]),
                    "hash":r["pair_hash"],"misses":int(r["misses"])}
    if set(measures)!=set(methods):
        raise ValueError(f"{replay}: requested methods missing: {set(methods)-set(measures)}")
    for frame in measures[methods[0]]:
        pairs={(v[frame]["pairs"],v[frame]["hash"])
               for v in (measures[m] for m in methods)}
        if len(pairs)!=1 or any(measures[m][frame]["misses"] for m in methods):
            raise ValueError(f"{replay}: exact candidate pairs mismatch frame {frame}")
    return (snap,replay,measures)

def wireframe(draw,box,convert,color):
    lo=box[:3];hi=box[3:]
    vertices=[
        (lo[0],lo[1],lo[2]),(hi[0],lo[1],lo[2]),
        (hi[0],hi[1],lo[2]),(lo[0],hi[1],lo[2]),
        (lo[0],lo[1],hi[2]),(hi[0],lo[1],hi[2]),
        (hi[0],hi[1],hi[2]),(lo[0],hi[1],hi[2])]
    mapped=[convert(p) for p in vertices]
    for a,b in ((0,1),(1,2),(2,3),(3,0),
                (4,5),(5,6),(6,7),(7,4),
                (0,4),(1,5),(2,6),(3,7)):
        draw.line((mapped[a],mapped[b]),fill=color,width=1)

def render_aabb(run_dir,out,methods=DEFAULT_METHODS,budget=512,
                seed=None,repeat=1,frames_per_regime=5,fps=6):
    run_dir=Path(run_dir)
    methods=tuple(methods)
    if not methods:raise ValueError("Need at least one method")
    datasets={}
    for regime in REGIMES:
        for material in MATERIALS:
            candidates=sorted((run_dir/"snapshots").glob(
                f"actual-{material}-{regime}-n{budget}-seed*.kmcb"))
            if seed is not None:
                candidates=[p for p in candidates
                            if p.stem.endswith(f"-seed{seed}")]
            if not candidates:
                raise FileNotFoundError(f"No frozen snapshot: {material} {regime}, budget {budget}, seed {seed}")
            chosen=None
            for p in candidates:
                seed_id=int(p.stem.split("-seed")[-1])
                chosen=scene_evidence(run_dir,material,regime,budget,
                                      seed_id,repeat,methods)
                if chosen:break
            if chosen is None:raise FileNotFoundError(f"No matching measured CSV for {material}/{regime}")
            datasets[(material,regime)]=chosen
    panel_w=280;panel_h=184
    width=panel_w*len(methods);height=88+panel_h*len(MATERIALS)+54
    result=[]
    for regime in REGIMES:
        per_material={}
        for material in MATERIALS:
            path,_,measures=datasets[(material,regime)]
            n,frame_total,_,_=snapshot_header(path)
            # Use actual post-warmup frames, not future replay or fabricated data.
            available=sorted(set.intersection(
                *(set(measures[m]) for m in methods)))
            available=[i for i in available if i>=min(16,max(0,frame_total//10))]
            if len(available)<frames_per_regime:raise ValueError("too few measured frames")
            # Fixed camera within a regime, avoids per-method camera bias.
            first,_=snapshot_frame(path,available[0])
            last,_=snapshot_frame(path,available[-1])
            coords=[p for b in first+last for p in (b[:3],b[3:])]
            world=bounds2d(coords)
            per_material[material]=(path,measures,available,world,n)
        for i in range(frames_per_regime):
            canvas=Image.new("RGB",(width,height),DARK);draw=ImageDraw.Draw(canvas)
            draw.text((18,12),f"KYOTO  |  {regime.upper()}  |  SHARED EXACT AABB SNAPSHOTS",
                      fill=WHITE,font=FH)
            draw.text((18,55),"Same geometry in every method cell. Only broad-phase timing / choice differ.",
                      fill=SUB,font=FS)
            for row,material in enumerate(MATERIALS):
                path,measures,available,world,n=per_material[material]
                f=available[round(i*(len(available)-1)/max(1,frames_per_regime-1))]
                boxes,_=snapshot_frame(path,f)
                for col,method in enumerate(methods):
                    x=col*panel_w;y=83+row*panel_h
                    rounded(draw,(x+5,y+5,x+panel_w-5,y+panel_h-5))
                    draw.text((x+13,y+12),material.upper(),font=FT,fill=COLORS[row])
                    draw.text((x+13,y+32),method.replace("_"," "),font=FS,fill=WHITE)
                    convert=mapper(world,(x+13,y+58,x+panel_w-13,y+panel_h-54))
                    for box in boxes:
                        wireframe(draw,box,convert,COLORS[row])
                    point=measures[method][f]
                    annotate(draw,x+13,y+panel_h-49,[
                        f"{point['ms']:.3f} ms  |  {point['pairs']} exact pairs",
                        f"Frame {f+1}  |  {n} actual boxes  |  missed {point['misses']}"
                    ])
            draw.text((18,height-35),
                      f"Regime {REGIMES.index(regime)+1}/6 • sampling {min(55,n)} boxes/panel to draw; pair counts include ALL boxes. Looping.",
                      font=FS,fill=SUB)
            result.append(canvas)
    return encode(result,out,fps)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest="mode",required=True)
    a=sub.add_parser("solver",help="real seven-policy physical mesh visual")
    a.add_argument("--input-dir",type=Path,required=True)
    a.add_argument("--output",type=Path,required=True)
    a.add_argument("--fps",type=int,default=9)
    a.add_argument("--stride",type=int,default=2)
    a.add_argument("--frame-limit",type=int,default=0)
    b=sub.add_parser("aabb",help="six materials x multiple methods, all six motion regimes")
    b.add_argument("--run-dir",type=Path,required=True)
    b.add_argument("--output",type=Path,required=True)
    b.add_argument("--methods",nargs="+",default=list(DEFAULT_METHODS))
    b.add_argument("--budget",type=int,choices=(128,512,2048),default=512)
    b.add_argument("--seed",type=int)
    b.add_argument("--repeat",type=int,default=1)
    b.add_argument("--frames-per-regime",type=int,default=5)
    b.add_argument("--fps",type=int,default=6)
    args=p.parse_args()
    if args.fps<1 or args.fps>60:raise SystemExit("fps must be 1..60")
    if args.mode=="solver":
        gif,mp4=render_solver(args.input_dir,args.output,args.fps,
                               args.stride,args.frame_limit)
    else:
        gif,mp4=render_aabb(args.run_dir,args.output,args.methods,
                            args.budget,args.seed,args.repeat,
                            args.frames_per_regime,args.fps)
    print("GIF (loop=forever):",gif)
    print("MP4 (loop in player):",mp4 if mp4 else "ffmpeg not available; GIF created")
    print("No cross-method geometry differences were invented.")

if __name__=="__main__":
    main()
