#!/usr/bin/env python3
"""Render REAL RBC five-policy solver mesh JSONL as a continuously looping GIF.

This renderer reads geometry written by ccp_rbc_coupled_tournament,
NOT inferred particle positions, synthetic folding, screenshots or AI
images. It fails if candidate policies disagree about actual vertex
positions, triangle connectivity or native self-contact corrections.
The same camera and scale are used in every panel and every frame.

Dependencies: Pillow. Rendering is POST-COMPUTE: graphics work is not
included in the native solver's step_ms measurement.

Usage:
  python3 scripts/render_rbc_physics_grid.py \
    --mesh-dir output/rbc-prepared-fold-contact-rich/mesh-g73013 \
    --output output/rbc-prepared-fold-contact-rich/real-five-policy.gif \
    --stride 2 --fps 10
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

POLICIES=("sap_rebuild","global_guard","frozen_v4",
          "rbc_indexed","rbc_flat")
TITLES={
    "sap_rebuild":"Rebuilt SAP",
    "global_guard":"Global Guard",
    "frozen_v4":"Frozen V4",
    "rbc_indexed":"RBC node index",
    "rbc_flat":"RBC flat repair",
}

def sha256(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for buf in iter(lambda:f.read(1<<20),b""):
            h.update(buf)
    return h.hexdigest()

def load(root:Path) -> dict:
    sources={}
    for policy in POLICIES:
        path=root/(policy+".jsonl")
        if not path.is_file():
            raise FileNotFoundError("Actual source mesh missing: "+str(path))
        data=[json.loads(line) for line in path.read_text("utf-8").splitlines()]
        if len(data)<3 or data[0].get("type")!="mesh_header":
            raise ValueError("Invalid native mesh header/trajectory")
        header=data[0]
        frames=data[1:]
        if header.get("policy")!=policy or header.get("schema")!=2:
            raise ValueError("Actual original mesh policy/schema mismatch")
        if [f.get("frame") for f in frames]!=list(range(len(frames))):
            raise ValueError("Original native mesh frames missing or reordered")
        count=len(header["initial_positions"])
        if count==0 or not header.get("triangles"):
            raise ValueError("Original solver mesh has no geometry")
        if any(len(f["positions"])!=count for f in frames):
            raise ValueError("Original solver mesh vertex counts changed")
        if len(header["triangle_sheets"])!=len(header["triangles"]):
            raise ValueError("Triangle sheet IDs not present")
        if any(any(i<0 or i>=count for i in tri) for tri in header["triangles"]):
            raise ValueError("Invalid source mesh triangle indices")
        if any(len(f.get("spheres",[]))!=2 for f in frames):
            raise ValueError("Original real sphere trajectories missing")
        for f in frames:
            metric=f.get("metrics",{})
            if metric.get("source_max_position_error_sap",math.inf)>1e-7:
                raise ValueError("Actual native solver diverged from SAP source")
            if metric.get("self_contacts",-1)<0:
                raise ValueError("Missing real same-sheet contact metrics")
            if any(not math.isfinite(v) for p in f["positions"] for v in p):
                raise ValueError("Nonfinite simulated source vertex")
        sources[policy]={"file":path,"sha256":sha256(path),
                         "header":header,"frames":frames}
    oracle=sources[POLICIES[0]]
    for name in POLICIES[1:]:
        other=sources[name]
        for field in ("triangles","triangle_sheets","initial_positions"):
            if oracle["header"][field]!=other["header"][field]:
                raise ValueError("Different source/topology between policies: "+field)
        if len(oracle["frames"])!=len(other["frames"]):
            raise ValueError("Different frame durations")
        for a,b in zip(oracle["frames"],other["frames"]):
            if a["metrics"]["self_contacts"]!=b["metrics"]["self_contacts"]:
                raise ValueError("Native self-contact event counters differ")
            for x,y in zip(a["positions"],b["positions"]):
                if max(abs(u-v) for u,v in zip(x,y))>1e-7:
                    raise ValueError("Native physics states are not identical")
    return sources

def iso(p:list[float])->tuple[float,float,float]:
    x,y,z=p
    return (0.81*x-0.57*y,0.38*x+0.39*y-0.83*z,
            0.46*x+0.55*y+0.45*z)

def render(sources:dict, output:Path, stride:int, fps:int,
           panel_width:int=440,panel_height:int=330)->dict:
    try:
        from PIL import Image,ImageDraw,ImageFont
    except ImportError as exc:
        raise RuntimeError("Install Pillow to render REAL native mesh data") from exc
    if output.exists():
        raise FileExistsError("Refusing to overwrite previously rendered evidence")
    if not output.name.endswith(".gif"):
        raise ValueError("Output path must end with .gif")
    oracle=sources[POLICIES[0]]
    triangles=oracle["header"]["triangles"]
    sheets=oracle["header"]["triangle_sheets"]
    times=list(range(0,len(oracle["frames"]),stride))
    if times[-1]!=len(oracle["frames"])-1:
        times.append(len(oracle["frames"])-1)
    # A GLOBAL fixed projection/scale across policies, frames, and panels.
    entire=[]
    for policy in POLICIES:
        entire.extend(iso(p) for f in sources[policy]["frames"]
                      for p in f["positions"])
    minx=min(p[0] for p in entire);maxx=max(p[0] for p in entire)
    miny=min(p[1] for p in entire);maxy=max(p[1] for p in entire)
    width=max(maxx-minx,1e-6);height=max(maxy-miny,1e-6)
    available_w=panel_width-44
    available_h=panel_height-110
    scale=min(available_w/width,available_h/height)
    def view(p):
        sx,sy,_=iso(p)
        return (int(panel_width/2+(sx-(minx+maxx)/2)*scale),
                int(55+available_h/2+(sy-(miny+maxy)/2)*scale))
    try:
        font=ImageFont.truetype("DejaVuSans.ttf",14)
        small=ImageFont.truetype("DejaVuSans.ttf",12)
    except OSError:
        font=ImageFont.load_default()
        small=font
    palette=((31,105,125),(184,115,75))
    count=len(oracle["frames"])
    images=[]
    for frame in times:
        sheet=Image.new("RGB",(panel_width*3,panel_height*2),(18,22,30))
        for pi,policy in enumerate(POLICIES):
            col,row=pi%3,pi//3
            sample=sources[policy]["frames"][frame]
            panel=Image.new("RGB",(panel_width,panel_height),(27,33,43))
            d=ImageDraw.Draw(panel)
            d.rectangle((1,1,panel_width-2,panel_height-2),
                        outline=(74,81,96),width=2)
            d.text((16,13),TITLES[policy],fill=(240,244,250),font=font)
            d.text((panel_width-140,14),f"Frame {frame+1:03d}/{count}",
                   fill=(180,190,203),font=small)
            # Source sphere centers and radii are genuine, not inferred.
            for sphere in sample["spheres"]:
                cx,cy=view(sphere["center"])
                rr=max(2,int(sphere["radius"]*scale*.40))
                d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),
                          fill=(72,82,101),outline=(106,130,170),width=2)
            pts=sample["positions"]
            order=sorted(range(len(triangles)),
                         key=lambda i:sum(iso(pts[j])[2] for j in triangles[i])/3)
            for i in order:
                ids=triangles[i]
                poly=[view(pts[k]) for k in ids]
                color=palette[sheets[i]%len(palette)]
                d.polygon(poly,fill=color)
                d.line(poly+[poly[0]],fill=(18,37,48),width=1)
            metric=sample["metrics"]
            d.text((15,panel_height-61),
                   f"Self-contact responses {metric['self_contacts']:,}  "
                   f"crossings {metric['self_crossings']:,}",
                   fill=(238,225,208),font=small)
            d.text((15,panel_height-39),
                   f"Full-step {metric['step_ms']:.3f} ms  "
                   f"broad {metric['broad_ms']:.3f} ms",
                   fill=(190,208,221),font=small)
            sheet.paste(panel,(col*panel_width,row*panel_height))
        d=ImageDraw.Draw(sheet)
        d.text((panel_width+15,panel_height+20),
               "EXACT SAME PHYSICAL STATE | Different charged candidate generators",
               fill=(246,241,228),font=font)
        d.text((panel_width+15,panel_height+45),
               "Prepared folded input - not spontaneous flat-cloth folding or CCD",
               fill=(192,198,212),font=small)
        images.append(sheet)
    output.parent.mkdir(parents=True,exist_ok=True)
    images[0].save(output,save_all=True,append_images=images[1:],
                   duration=max(40,int(1000/fps)),loop=0,optimize=False)
    preview=output.with_suffix(".png")
    if preview.exists():
        raise FileExistsError("Refusing to overwrite original preview PNG")
    images[min(2,len(images)-1)].save(preview)
    manifest={
        "schema":"rbc-real-mesh-evidence-render-v1",
        "real_native_source_only":True,
        "frames_in_original_native_solver":count,
        "rendered_frames":len(times),
        "frame_indices":times,
        "same_camera_and_scale_all_five_policies":True,
        "max_source_state_difference_sap":1e-7,
        "self_contact_is_true_native_solver_counter":True,
        "zero_overwrite":True,
        "not_independent_ccd_or_penetration_oracle":True,
        "native_sources_sha256":{m:sources[m]["sha256"] for m in POLICIES},
        "gif_sha256":sha256(output),"preview_sha256":sha256(preview)
    }
    manifest_path=output.with_suffix(".json")
    if manifest_path.exists():
        raise FileExistsError("Refusing to overwrite real source render manifest")
    manifest_path.write_text(json.dumps(manifest,indent=2)+"\n")
    return manifest

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mesh-dir",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--stride",type=int,default=2)
    p.add_argument("--fps",type=int,default=10)
    a=p.parse_args()
    if not 1<=a.stride<=20 or not 1<=a.fps<=30:
        p.error("Stride must be 1..20 and fps 1..30")
    source=load(a.mesh_dir)
    result=render(source,a.output,a.stride,a.fps)
    print("PASS true five-policy physics visualization",
          result["rendered_frames"],"frames, sha256",result["gif_sha256"])

if __name__=="__main__":
    main()
