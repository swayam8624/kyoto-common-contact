#!/usr/bin/env python3
"""Convert external animated triangle OBJ sequences to Kyoto frozen AABB ABI.

Use for ARCSim, CLOTH3D, RGBench *only when actual per-frame OBJ mesh
sequences exist*. RGBench's released pointcloud observations are not
automatically valid triangle-mesh trajectories.

One stable triangle ID per face across ALL frames; any topology change
fails loudly. Data stays in original source units (expected meters).
This is an external BROAD-PHASE geometry benchmark, not a sim-to-real
cloth fidelity score or end-to-end solver test.

Produces KYOTOM01 with N=triangles, frame-count, seed and zero-based
triangle bounding boxes padded by an explicit half margin. All outputs
are immutable, checksummed, and have provenance metadata.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path

MATERIAL_CODE={"cloth":0,"spring":1,"soft_ball":2,"sponge":3,"cushion":4,"mixed":5}

def natural_key(path):
    return [int(x) if x.isdigit() else x.lower()
            for x in re.split(r"(\d+)",path.name)]

def sha(path):
    hasher=hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda:stream.read(1<<20),b""):
            hasher.update(block)
    return hasher.hexdigest()

def obj_frame(path):
    points=[];triangles=[]
    with Path(path).open(encoding="utf-8",errors="replace") as stream:
        for number,line in enumerate(stream,1):
            if line.startswith("v "):
                fields=line.split()
                if len(fields)<4:
                    raise ValueError(f"Invalid vertex at {path}:{number}")
                xyz=tuple(float(q) for q in fields[1:4])
                if not all(math.isfinite(v) for v in xyz):
                    raise ValueError(f"Nonfinite OBJ vertex: {path}:{number}")
                points.append(xyz)
            elif line.startswith("f "):
                raw=line.split()[1:]
                face=[]
                for item in raw:
                    vertex=int(item.split("/")[0])
                    idx=vertex-1 if vertex>0 else len(points)+vertex
                    if idx<0 or idx>=len(points):
                        raise ValueError(f"Bad OBJ vertex index: {path}:{number}")
                    face.append(idx)
                if len(face)<3:raise ValueError("Face with fewer than three vertices")
                for j in range(1,len(face)-1):
                    tri=(face[0],face[j],face[j+1])
                    if len(set(tri))!=3:raise ValueError("Degenerate OBJ face topology")
                    triangles.append(tri)
    if not points or not triangles:
        raise ValueError(f"OBJ frame has no triangle mesh: {path}")
    return points,triangles

def convert(directory,output,seed,material,margin,max_frames):
    paths=sorted(Path(directory).glob("*.obj"),key=natural_key)
    if max_frames:paths=paths[:max_frames]
    if len(paths)<2:
        raise ValueError("An external mesh sequence requires >=2 OBJ frames; "
                         "one static garment OBJ is NOT a motion benchmark")
    if output.exists() or output.with_suffix(".json").exists():
        raise ValueError("Refusing to overwrite external evidence/manifest")
    output.parent.mkdir(parents=True,exist_ok=True)
    topology=None;vertex_count=None;reference=[]
    with output.open("wb") as out:
        out.write(b"KYOTOM01")
        # Header is overwritten after validating first frame below, using
        # seek. Never accept changing triangle topology silently.
        out.write(b"\x00"*16)
        for frame_index,path in enumerate(paths):
            vertices,faces=obj_frame(path)
            if topology is None:
                topology=faces;vertex_count=len(vertices)
                if len(faces)>50000:
                    raise ValueError("Kyoto replay supports <=50000 primitives")
                out.seek(8)
                out.write(struct.pack("<4I",len(faces),len(paths),seed,
                                      MATERIAL_CODE[material]))
                out.seek(0,2)
            elif faces!=topology or len(vertices)!=vertex_count:
                raise ValueError(f"External mesh topology changed at frame {frame_index}: {path}; "
                                 "use fixed-mesh ARCSim or separately study remeshing")
            for t in topology:
                xs=[vertices[v][0] for v in t]
                ys=[vertices[v][1] for v in t]
                zs=[vertices[v][2] for v in t]
                values=(min(xs)-margin,min(ys)-margin,min(zs)-margin,
                        max(xs)+margin,max(ys)+margin,max(zs)+margin)
                if not all(math.isfinite(float(v)) for v in values):
                    raise ValueError("Nonfinite padded triangle bounds")
                out.write(struct.pack("<6f",*values))
    manifest={
        "schema":"kyoto-external-obj-abi-v1","source_directory":str(Path(directory).resolve()),
        "material_label":material,"seed":seed,"frames":len(paths),
        "triangles":len(topology),"vertices":vertex_count,
        "half_margin_m":margin,"topology_fixed":True,
        "source_files":[{"path":str(p),"sha256":sha(p)} for p in paths],
        "output_sha256":sha(output),
        "warning":"This records triangle AABB candidate sets only; adjacency contact filters, "
                  "actual triangle/edge intersections, solver time, residual penetration and "
                  "sim-to-real quality must be evaluated separately.",
    }
    output.with_suffix(".json").write_text(json.dumps(manifest,indent=2)+"\n")
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("directory",type=Path,help="Directory of sequential fixed-topology OBJ frames")
    p.add_argument("output",type=Path,help="New .kmcb path")
    p.add_argument("--material",choices=MATERIAL_CODE,default="cloth")
    p.add_argument("--seed",type=int,default=7001)
    p.add_argument("--half-margin-m",type=float,default=.0125)
    p.add_argument("--max-frames",type=int,default=0)
    a=p.parse_args()
    if a.output.suffix.lower()!=".kmcb" or not (1<=a.seed<=2000000000) or (
        a.max_frames<0 or a.max_frames>10000 or not math.isfinite(a.half_margin_m) or
        not 0<=a.half_margin_m<=1):
        p.error("invalid output .kmcb, seed, max frames or padding")
    m=convert(a.directory,a.output,a.seed,a.material,a.half_margin_m,a.max_frames)
    print("[EXTERNAL] frozen mesh broad-phase input",m["frames"],
          "frames,",m["triangles"],"triangles, sha256",m["output_sha256"])

if __name__=="__main__":
    main()
