#!/usr/bin/env python3
"""Import a fixed-topology triangulated OBJ time series into exact KYOTOM01 AABBs.

For ARCSim/CLOTH3D or other external data AFTER it has legally been exported
to sequential OBJ frames with identical vertex and triangle topology.
We NEVER download/reproduce third-party datasets or infer material physics.

This preserves actual sampled geometries; each triangle becomes one 3D AABB.
It is for discrete snapshot broad-phase comparison, NOT CCD, precise contacts,
reference motion reproduction or garment quality evaluation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
import tempfile
from pathlib import Path

HEADER=struct.Struct("<8sIIII")
BOX=struct.Struct("<6f")
MATERIALS={"cloth":0,"sponge":1,"cushion":2,"soft_ball":3,"spring":4,"mixed":5}

def parse_obj(path):
    vertices=[];faces=[]
    with Path(path).open("r",encoding="utf-8") as fd:
        for line in fd:
            values=line.strip().split()
            if not values or values[0].startswith("#"):continue
            if values[0]=="v" and len(values)>=4:
                xyz=tuple(float(x) for x in values[1:4])
                if any(not math.isfinite(x) or abs(x)>1e12 for x in xyz):
                    raise ValueError(f"{path}: invalid world position")
                vertices.append(xyz)
            elif values[0]=="f":
                if len(values)!=4:
                    raise ValueError(f"{path}: only explicitly triangulated faces accepted")
                face=[]
                for token in values[1:]:
                    idx=int(token.split("/")[0])
                    if idx<=0 or idx>len(vertices):
                        raise ValueError(f"{path}: nonpositive, forward or invalid OBJ index")
                    face.append(idx-1)
                if len(set(face))!=3:
                    raise ValueError(f"{path}: degenerate triangle vertex IDs")
                faces.append(tuple(face))
    if not vertices or not faces:
        raise ValueError(f"{path}: missing vertices or triangles")
    return vertices,faces

def file_sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1<<20),b""):
            h.update(block)
    return h.hexdigest()

def convert(input_dir,outfile,material="cloth",padding=0.,seed=9001,pattern="*.obj"):
    input_dir=Path(input_dir)
    files=sorted(input_dir.glob(pattern))
    if not 2<=len(files)<=10000:
        raise ValueError("Provide 2..10,000 OBJ frames in natural zero-padded order")
    if material not in MATERIALS:
        raise ValueError("Unknown material tag (not inferred from geometry)")
    if not math.isfinite(padding) or padding<0 or padding>1e6:
        raise ValueError("Invalid nonnegative padding")
    first_v,faces=parse_obj(files[0])
    if len(faces)>50000:raise ValueError("More than 50,000 triangles")
    outfile=Path(outfile)
    outfile.parent.mkdir(parents=True,exist_ok=True)
    manifest={
        "format":"KYOTOM01","schema":1,"source_kind":"external_fixed_topology_OBJ_sequence",
        "material_label":material,"triangle_count":len(faces),
        "vertex_count":len(first_v),"frames":len(files),
        "seed_label":seed,"aabb_padding_units":padding,
        "frames_sha256":{},"limitations":[
            "AABB snapshots only; no continuous collision guarantee",
            "No solver speed or material fidelity conclusion from OBJ replay",
            "No contact filters, vertex-edge CCD or topology changes represented",
            "Input must be pre-exported under the source dataset's license",
        ],
    }
    # Atomically publish only after every topology and bounding validation passes.
    fd,tmp=tempfile.mkstemp(prefix=outfile.name+".",suffix=".partial",dir=outfile.parent)
    os.close(fd)
    try:
        with open(tmp,"wb") as out:
            out.write(HEADER.pack(b"KYOTOM01",len(faces),len(files),seed,MATERIALS[material]))
            for path in files:
                vertices,cur_faces=parse_obj(path)
                if len(vertices)!=len(first_v) or cur_faces!=faces:
                    raise ValueError(f"{path}: topological change; needs remeshing-aware adapter")
                for a,b,c in faces:
                    values=(vertices[a],vertices[b],vertices[c])
                    lo=[min(v[k] for v in values)-padding for k in range(3)]
                    hi=[max(v[k] for v in values)+padding for k in range(3)]
                    out.write(BOX.pack(*(lo+hi)))
                manifest["frames_sha256"][path.name]=file_sha(path)
        os.replace(tmp,outfile)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)
    manifest["kmcb_sha256"]=file_sha(outfile)
    mpath=outfile.with_suffix(".manifest.json")
    mpath.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(f"EXTERNAL_OBJ_IMPORTED {len(files)} frames {len(faces)} triangle AABBs {outfile}")
    print("MANIFEST",mpath)
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("input_dir",type=Path)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--pattern",default="*.obj")
    p.add_argument("--material",choices=list(MATERIALS),default="cloth")
    p.add_argument("--seed",type=int,default=9001)
    p.add_argument("--padding",type=float,default=0.)
    a=p.parse_args()
    convert(a.input_dir,a.output,a.material,a.padding,a.seed,a.pattern)

if __name__=="__main__":main()
