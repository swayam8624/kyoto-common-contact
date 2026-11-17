#!/usr/bin/env python3
"""Real-observation geometric deviation for *already aligned* simulated meshes.

Accepts registered, time-synchronized PCD/PLY reference point clouds and
fixed-topology OBJ simulation frames. Reports symmetric RMS nearest-neighbor
surface distance; this is NOT the official RGBench metric or a substitute for
collision/CCD correctness. Requires explicit user assertion of calibration.

No remote dataset download, invented registration, or rescaling to make a
prediction look better. Optional scipy KDTree; a tiny pure-Python tree fallback
works without external dependencies.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
import random
import struct
from pathlib import Path
from statistics import mean

try:
    from scipy.spatial import cKDTree as ScipyTree
except ImportError:
    ScipyTree=None

try:
    from .import_mesh_sequence import parse_obj,file_sha
except ImportError:
    from import_mesh_sequence import parse_obj,file_sha

def read_pcd(path):
    with Path(path).open("rb") as fd:
        meta={}
        while True:
            line=fd.readline()
            if not line:raise ValueError(f"{path}: truncated PCD header")
            text=line.decode("ascii").strip()
            if not text or text.startswith("#"):continue
            fields=text.split()
            meta[fields[0].upper()]=fields[1:]
            if fields[0].upper()=="DATA":break
        layout=meta.get("FIELDS") or meta.get("FIELD")
        if not layout or not {"x","y","z"}.issubset(layout):
            raise ValueError(f"{path}: PCD requires x,y,z FIELDS")
        dtype=meta.get("DATA",[""])[0].lower()
        if dtype not in {"ascii","binary"}:
            raise ValueError(f"{path}: unsupported PCD DATA {dtype}")
        count=int(meta.get("POINTS",["0"])[0])
        if not 0<count<50_000_000:raise ValueError("bad PCD point count")
        xyz=[layout.index(axis) for axis in ("x","y","z")]
        if dtype=="ascii":
            points=[]
            for raw in fd:
                if not raw.strip():continue
                vals=raw.decode("ascii").split()
                points.append(tuple(float(vals[i]) for i in xyz))
        else:
            sizes=[int(x) for x in meta.get("SIZE",["4"]*len(layout))]
            types=meta.get("TYPE",["F"]*len(layout))
            counts=[int(x) for x in meta.get("COUNT",["1"]*len(layout))]
            if len(sizes)!=len(layout) or len(types)!=len(layout) or len(counts)!=len(layout):
                raise ValueError("bad PCD SIZE/TYPE/COUNT specification")
            if any(v!=1 for v in counts):
                raise ValueError("multi-count PCD fields require a dedicated adapter")
            offsets=[];offset=0
            for size in sizes:offsets.append(offset);offset+=size
            stride=offset
            payload=fd.read()
            if len(payload)<count*stride:raise ValueError("truncated binary PCD")
            points=[]
            for k in range(count):
                coords=[]
                for j in xyz:
                    if types[j]!="F" or sizes[j] not in (4,8):
                        raise ValueError("binary PCD x/y/z must be floats")
                    fmt="<f" if sizes[j]==4 else "<d"
                    coords.append(struct.unpack_from(fmt,payload,k*stride+offsets[j])[0])
                points.append(tuple(coords))
    if len(points)!=count:
        raise ValueError(f"{path}: declared {count} points, read {len(points)}")
    return validate_points(points,path)

def read_ply(path):
    with Path(path).open("rb") as fd:
        header=[]
        while True:
            line=fd.readline()
            if not line:raise ValueError("truncated PLY header")
            text=line.decode("ascii",errors="strict").strip()
            header.append(text)
            if text=="end_header":break
        if header[0]!="ply" or "format ascii 1.0" not in header:
            raise ValueError(f"{path}: only ASCII PLY supported (convert binary PLY first)")
        count=0;fields=[];inside=False
        for line in header:
            if line.startswith("element "):
                parts=line.split();inside=parts[1]=="vertex"
                if inside:count=int(parts[2])
            elif inside and line.startswith("property "):
                fields.append(line.split()[-1])
        if not {"x","y","z"}.issubset(fields):
            raise ValueError("PLY lacks xyz properties")
        xyz=[fields.index(x) for x in ("x","y","z")]
        points=[]
        for unused in range(count):
            vals=fd.readline().decode("ascii").split()
            if not vals:raise ValueError("truncated vertex list")
            points.append(tuple(float(vals[i]) for i in xyz))
    return validate_points(points,path)

def validate_points(points,path):
    if not points:raise ValueError(f"{path}: zero reference points")
    if any(any(not math.isfinite(v) for v in p) for p in points):
        raise ValueError(f"{path}: nonfinite reference geometry")
    return points

def vec_dist_sq(a,b):
    return sum((a[k]-b[k])**2 for k in range(3))

class KD:
    def __init__(self,points,depth=0):
        if not points:self.p=None;return
        ax=depth%3
        points=sorted(points,key=lambda p:p[ax])
        mid=len(points)//2
        self.p=points[mid];self.ax=ax
        self.left=KD(points[:mid],depth+1) if mid else None
        self.right=KD(points[mid+1:],depth+1) if mid+1<len(points) else None
    def squared(self,query,best=float("inf")):
        if self.p is None:return best
        best=min(best,vec_dist_sq(query,self.p))
        d=query[self.ax]-self.p[self.ax]
        first=self.left if d<0 else self.right
        last=self.right if d<0 else self.left
        if first:best=first.squared(query,best)
        if last and d*d<best:best=last.squared(query,best)
        return best

def nearest_sq(source,target):
    if ScipyTree is not None:
        d,_=ScipyTree(target).query(source,k=1,workers=1)
        return [float(x*x) for x in d]
    root=KD(list(target))
    return [root.squared(p) for p in source]

def downsample(points,limit):
    if len(points)<=limit:return points
    return [points[round(i*(len(points)-1)/(limit-1))] for i in range(limit)]

def surface_samples(vertices,triangles,target,seed=17):
    # Area-weighted barycentric sampling rather than treating a vertex-cloud
    # as a uniformly sampled cloth surface.
    areas=[];positive=[];total=0.
    for tri in triangles:
        a,b,c=(vertices[i] for i in tri)
        ux,uy,uz=(b[k]-a[k] for k in range(3))
        vx,vy,vz=(c[k]-a[k] for k in range(3))
        area=.5*math.sqrt((uy*vz-uz*vy)**2+(uz*vx-ux*vz)**2+(ux*vy-uy*vx)**2)
        if area<=1e-14:continue
        total+=area;areas.append(total);positive.append(tri)
    if not positive:raise ValueError("No non-degenerate simulated mesh triangles")
    rng=random.Random(seed)
    sampled=[]
    for unused in range(target):
        tri=positive[min(len(positive)-1,bisect.bisect_left(areas,rng.random()*total))]
        a,b,c=(vertices[i] for i in tri)
        r=math.sqrt(rng.random());s=rng.random()
        w=(1-r,r*(1-s),r*s)
        sampled.append(tuple(w[0]*a[k]+w[1]*b[k]+w[2]*c[k] for k in range(3)))
    return sampled

def evaluate(reference_dir,simulated_dir,units,confirmed,limit=2048):
    if not confirmed:
        raise ValueError("Requires --confirm-world-aligned; no automatic coordinate/time alignment")
    if not units:raise ValueError("Explicit unit label required")
    if limit<32 or limit>20000:raise ValueError("Sample budget must be 32..20,000")
    ref=sorted([*Path(reference_dir).glob("*.pcd"),
                *Path(reference_dir).glob("*.ply")])
    pred=sorted(Path(simulated_dir).glob("*.obj"))
    if len(ref)!=len(pred) or len(ref)<2:
        raise ValueError("Need matching >=2 frames of aligned reference clouds and simulated meshes")
    rows=[];topology=None
    for frame,(ref_file,mesh_file) in enumerate(zip(ref,pred)):
        actual=read_pcd(ref_file) if ref_file.suffix==".pcd" else read_ply(ref_file)
        vertices,faces=parse_obj(mesh_file)
        if topology is None:topology=(len(vertices),faces)
        if topology!=(len(vertices),faces):
            raise ValueError("Changing simulated topology requires separate registration")
        actual=downsample(actual,limit)
        surface=surface_samples(vertices,faces,len(actual),seed=19+frame)
        one=nearest_sq(surface,actual)
        two=nearest_sq(actual,surface)
        rows.append({
            "frame":frame,"real_file":ref_file.name,"sim_file":mesh_file.name,
            "real_sha256":file_sha(ref_file),"sim_sha256":file_sha(mesh_file),
            "real_points_used":len(actual),"surface_samples_used":len(surface),
            "symmetric_rms_nearest_neighbor":math.sqrt((mean(one)+mean(two))*.5),
            "max_directed_nearest_neighbor":max(math.sqrt(max(one)),math.sqrt(max(two))),
        })
    return {
        "method":"symmetric RMS nearest-neighbor surface/observation distance",
        "unit":units,"frames":rows,
        "mean_symmetric_rms":mean(r["symmetric_rms_nearest_neighbor"] for r in rows),
        "limitations":[
            "User asserted time-synchronized, world-aligned point clouds; not auto-verified",
            "This is NOT official RGBench scoring; do not compare reported numbers directly",
            "Sampled surface introduces Monte Carlo error and points are not weighted for occlusion",
            "Reference observations may have occlusions, registration uncertainty and depth noise",
            "This does not test contact force, CCD, fold topology, or physical material calibration",
        ]
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reference-dir",type=Path,required=True)
    p.add_argument("--simulated-dir",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--units",required=True,help="Explicit coordinate units e.g. meters")
    p.add_argument("--confirm-world-aligned",action="store_true")
    p.add_argument("--max-points",type=int,default=2048)
    a=p.parse_args()
    report=evaluate(a.reference_dir,a.simulated_dir,a.units,
                    a.confirm_world_aligned,a.max_points)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2)+"\n")
    print("Real-observation geometry comparison saved",a.output)
    print("Mean symmetric RMS",report["mean_symmetric_rms"],a.units)
    print("Not the official RGBench benchmark; see limitations")

if __name__=="__main__":main()
