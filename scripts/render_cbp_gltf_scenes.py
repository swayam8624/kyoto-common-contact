#!/usr/bin/env python3
"""Render REAL Khronos glTF triangle-geometry collision benchmark offline.

Uses imported static GLB mesh triangles, their exact recorded rigid motions,
full exact CPU oracle pair receipts and optional separate Metal replay. Does
NOT pretend to run glTF skinning, animations, deformable physics, narrow phase,
or candidate-pair to mesh-contact conversion.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics

def rows(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as f:
        result = list(csv.DictReader(f))
    if not result:
        raise ValueError("Missing CSV evidence " + str(path))
    return result

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("scene",type=Path)
    p.add_argument("--html",type=Path,required=True)
    p.add_argument("--preview-only",action="store_true")
    args=p.parse_args()
    if args.html.exists():
        raise FileExistsError("Refusing to overwrite an existing asset scene report")
    scene=args.scene
    mesh_path=scene/"asset-meshes.json"
    if mesh_path.stat().st_size > 200*1024*1024:
        raise ValueError("Asset geometry exceeds browser payload budget")
    imported=json.loads(mesh_path.read_text(encoding="utf-8"))
    if imported["schema"]!="CCP_REAL_GLTF_MOTION_V1":
        raise ValueError("Missing original GLB triangle geometry metadata")
    models=imported["objects"]
    frames=imported["frames"]
    if not (12<=len(models)<=26 and 12<=len(frames)<=240):
        raise ValueError("Unsupported real model cohort size")
    receipt=rows(scene/"frames.csv")
    instance_rows=rows(scene/"instances.csv")
    if len(receipt)!=len(frames) or len(instance_rows)!=len(models):
        raise ValueError("Triangle scene replay frame/instance metadata mismatch")
    for i,m in enumerate(models):
        count=m["selected_triangles"]
        display_count=m.get("display_triangles", count)
        visual=m.get("visual_triangles",m["triangles"])
        if (m["name"] != instance_rows[i]["kind"] or
            len(m["triangles"])!=count or not 1<=count<=2000 or
            not 1<=display_count<=6000 or len(visual)!=display_count or
            m["source_triangles"]<max(count,display_count) or
            not isinstance(m["sha256"],str) or len(m["sha256"])!=64):
            raise ValueError("Imported collision/display mesh provenance mismatch")
        for source_mesh in (m["triangles"],visual):
            if not all(len(t)==9 and all(math.isfinite(v) for v in t)
                       for t in source_mesh):
                raise ValueError("Invalid source GLB triangle vertex positions")
    for index, frame in enumerate(frames):
        if len(frame)!=len(models) or any(
            len(pose)!=4 or not all(math.isfinite(v) for v in pose)
            for pose in frame
        ):
            raise ValueError(f"Rigid transform receipt invalid at frame {index}")
    cpu=[] if args.preview_only else rows(scene/"cpu-core.csv")
    by_frame=[{} for _ in frames]
    for row in cpu:
        f=int(row["frame"])
        if not 0<=f<len(frames):
            raise ValueError("CPU benchmark frame exceeds geometry")
        mode=row["mode"]
        if mode in by_frame[f] or row["exact_sap_equal"]!="1":
            raise ValueError("Duplicate or nonexact CPU method")
        by_frame[f][mode]={
            "mode":mode,"host":float(row["ms"]),"gpu":None,
            "pairs":int(row["pairs"]), "inter":int(row["inter_instance_pairs"]),
            "hash":str(row["pair_hash"]),"is_gpu":False
        }
    metal=sorted(scene.glob("metal-*.csv")) if not args.preview_only else []
    for path in metal:
        for row in rows(path):
            f=int(row["frame"])
            mode=row["backend"]
            if not 0<=f<len(frames) or mode in by_frame[f] or int(row["misses"])!=0:
                raise ValueError("Incomplete/incorrect Metal mode")
            by_frame[f][mode]={
                "mode":mode,"host":float(row["host_ms"]),
                "gpu":float(row["gpu_ms"]),
                "pairs":int(row["overlaps"]),"inter":None,
                "hash":str(row["pair_hash"]),"is_gpu":True
            }
    if not args.preview_only:
        method_set=set(by_frame[0])
        if "sap_rebuild" not in method_set or "frozen_v4" not in method_set:
            raise ValueError("Missing exact CPU SAP/frozen V4 reference")
        for i,group in enumerate(by_frame):
            if set(group)!=method_set:
                raise ValueError(f"Incomplete method set at frame {i}")
            ref=group["sap_rebuild"]
            for row in group.values():
                if (row["pairs"],row["hash"])!=(ref["pairs"],ref["hash"]):
                    raise ValueError("CPU/Metal complete candidate digest mismatch")
                if not math.isfinite(row["host"]) or row["host"]<0:
                    raise ValueError("Invalid measured algorithm time")
                if row["gpu"] is not None and (
                        not math.isfinite(row["gpu"]) or row["gpu"]<0):
                    raise ValueError("Invalid GPU kernel time")
    methods=[]
    if not args.preview_only:
        for method in sorted(by_frame[0]):
            values=[frame[method] for frame in by_frame[8:]]
            if not values:values=[f[method] for f in by_frame]
            times=sorted(v["host"] for v in values)
            kernel=[v["gpu"] for v in values if v["gpu"] is not None]
            methods.append({
                "name":method,"is_gpu":values[0]["is_gpu"],
                "mean":statistics.fmean(times),
                "p95":times[max(0,math.ceil(len(times)*.95)-1)],
                "kernel":statistics.fmean(kernel) if kernel else None
            })
    # Parse one bounded preview file and keep original room-scale framing.
    previews=[json.loads(x) for x in
              (scene/"preview.jsonl").read_text(encoding="utf-8").splitlines() if x.strip()]
    if len(previews)!=len(frames):
        raise ValueError("Scene preview must preserve all source frame envelopes")
    lower=[min(b[1+k] for f in previews for b in f["instances"])
           for k in range(3)]
    upper=[max(b[4+k] for f in previews for b in f["instances"])
           for k in range(3)]
    spatial=[]
    hotspots=scene/"contact-hotspots.jsonl"
    if hotspots.is_file() and not args.preview_only:
        spatial=[json.loads(line) for line in
                 hotspots.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(spatial)!=len(frames):
            raise ValueError("Contact candidate location frame mismatch")
        for i,entry in enumerate(spatial):
            ref=by_frame[i]["sap_rebuild"]
            if (entry["frame"]!=i or str(entry["pair_hash"])!=ref["hash"]
                or entry["pairs"]!=ref["pairs"] or
                entry["inter_pairs"]!=ref["inter"]):
                raise ValueError("Spatial hotspot locations failed exact SAP receipt")
    payload={
        "models":models,"poses":frames,"runs":by_frame,"stats":methods,
        "previews":previews,"spatial":spatial,
        "limits":[lower,upper],
        "preview_only":args.preview_only,
        "layout":receipt[0]["layout"],"seed":int(receipt[0]["seed"]),
        "primitives":sum(m["selected_triangles"] for m in models),
        "triangles_display":sum(m.get("display_triangles",m["selected_triangles"]) for m in models),
        "triangles_original":sum(m["source_triangles"] for m in models),
        "boundary":("Static source mesh sampled triangles with rigid trajectory; "
                    "not full source mesh, glTF animation, deformable mesh or coupled contact solver."),
    }
    text=json.dumps(payload,separators=(",",":"),allow_nan=False)
    text=text.replace("<","\\u003c").replace("&","\\u0026")
    template=Path(__file__).resolve().parent.parent/"viewer"/"gltf_scene_observatory.html"
    html=template.read_text(encoding="utf-8")
    if html.count("__PAYLOAD__")!=1:
        raise ValueError("Incorrect glTF benchmark viewer template")
    args.html.parent.mkdir(parents=True,exist_ok=True)
    args.html.write_text(html.replace("__PAYLOAD__",text),encoding="utf-8")
    print(f"PASS real glTF scene observatory: {len(models)} named models, "
          f"{payload['primitives']} collision triangles, "
          f"{payload['triangles_display']} real visual triangles, "
          f"{payload['triangles_original']} source triangles, "
          f"{len(frames)} rigid-motion frames, "
          f"{len(methods)} verified CPU/Metal policies")
    print("Open offline 3D source-mesh viewer: "+str(args.html))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
