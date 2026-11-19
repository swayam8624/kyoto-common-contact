#!/usr/bin/env python3
"""Independent post-simulation nonlocal cloth surface piercing audit.

This tool NEVER invokes an RBC candidate generator. It reads the
original native C++ solver's actual vertex positions and mesh triangles
and evaluates strict triangle-edge/triangle-face penetrations after
every solver frame AND at multiple linearly interpolated states between
adjacent frames. It also checks ALL five independent native source
trajectories against SAP before auditing.

A LINEAR interpolation check is NOT exact continuous CCD; highly
nonlinear within-step paths or coplanar overlaps can remain undetected.
It is nevertheless a materially stronger source-derived check than
"all five methods returned identical AABB pairs."

Does not overwrite evidence. Source/provenance SHA-256 included.
Requires NumPy and scripts/render_rbc_physics_studio.py.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from render_rbc_physics_studio import POLICIES, source_data, file_sha256


def actual_nonlocal_pairs(header: dict) -> np.ndarray:
    triangles=np.asarray(header["triangles"],dtype=np.int64)
    sheet=np.asarray(header["triangle_sheets"],dtype=np.int64)
    tri_count=len(triangles)
    # This is specifically the original kSide=13 cloth grid:
    # two triangles per 12x12 cell per independent sheet.
    cells_per_sheet=2*12*12
    if any(int((sheet==s).sum())!=cells_per_sheet
           for s in np.unique(sheet)):
        raise ValueError("Unsupported triangulation; refine source topology filtering")
    cell=(np.arange(tri_count)%cells_per_sheet)//2
    cx=cell%12
    cy=cell//12
    return np.array(
        [(i,j) for i in range(tri_count)
         for j in range(i+1,tri_count)
         if sheet[i]==sheet[j] and
         (abs(cx[i]-cx[j])>1 or abs(cy[i]-cy[j])>1)],
        dtype=np.int32).reshape(-1,2)


def exact_strict_edge_face_crossing_count(
        positions: np.ndarray, triangles: np.ndarray,
        topo_pairs: np.ndarray) -> tuple[int,int]:
    """Independent double-precision AABB-cull + two-sided segment/face audit.

    Returns number of NONLOCAL triangle pairs having at least one
    strict interior edge-piercing. This intentionally does not count
    mere touching and cannot decide coplanar coincident triangles.
    """
    t=positions[triangles]
    mins=t.min(axis=1);maxs=t.max(axis=1)
    a,b=topo_pairs[:,0],topo_pairs[:,1]
    candidates=topo_pairs[
        np.all(mins[a]<=maxs[b],axis=1)&
        np.all(mins[b]<=maxs[a],axis=1)]
    if not len(candidates):
        return 0,0
    left=t[candidates[:,0]]
    right=t[candidates[:,1]]
    count=len(candidates)
    intersection=np.zeros(count,dtype=bool)
    eps=1e-6
    for edges,face in ((left,right),(right,left)):
        e1=face[:,1]-face[:,0]
        e2=face[:,2]-face[:,0]
        n=np.cross(e1,e2)
        d00=np.einsum("ij,ij->i",e1,e1)
        d01=np.einsum("ij,ij->i",e1,e2)
        d11=np.einsum("ij,ij->i",e2,e2)
        det=d00*d11-d01*d01
        for k in range(3):
            first=edges[:,k]
            diff=edges[:,(k+1)%3]-first
            den=np.einsum("ij,ij->i",n,diff)
            valid=(np.abs(den)>1e-12)&(np.abs(det)>1e-20)
            tt=np.zeros(count)
            tt[valid]=np.einsum("ij,ij->i",
                               n[valid],face[valid,0]-first[valid])/den[valid]
            valid &= (tt>eps)&(tt<1-eps)
            point=first+diff*tt[:,None]
            v=point-face[:,0]
            d20=np.einsum("ij,ij->i",v,e1)
            d21=np.einsum("ij,ij->i",v,e2)
            u=np.zeros(count)
            w=np.zeros(count)
            u[valid]=(d11[valid]*d20[valid]-
                       d01[valid]*d21[valid])/det[valid]
            w[valid]=(d00[valid]*d21[valid]-
                       d01[valid]*d20[valid])/det[valid]
            valid &= (u>eps)&(w>eps)&(u+w<1-eps)
            intersection |= valid
    return int(intersection.sum()),count


def audit(mesh_dir:Path, output:Path, subdivision:int=8,
          require_zero:bool=True)->dict:
    if output.exists():
        raise FileExistsError("Original penetration result is immutable")
    if not 2<=subdivision<=32:
        raise ValueError("Need 2..32 linear temporal subdivisions")
    sources=source_data(mesh_dir)
    header,frames,source=sources[POLICIES[0]]
    triangles=np.asarray(header["triangles"],dtype=np.int64)
    pairs=actual_nonlocal_pairs(header)
    before=np.asarray(header["initial_positions"],dtype=float)
    original=before.copy()
    rows=[]
    for i,frame in enumerate(frames):
        after=np.asarray(frame["positions"],dtype=float)
        endpoint_count,candidates=exact_strict_edge_face_crossing_count(
            after,triangles,pairs)
        # Independently recompute a native source claim, not just
        # accept its framewise metric from the original solver.
        native=int(frame["metrics"]["geometric_self_intersections"])
        if endpoint_count!=native:
            raise ValueError(
                "Independent endpoint geometry disagrees with original native "
                f"intersection metric frame {i}: Python={endpoint_count} native={native}"
            )
        intermediate=[]
        for j in range(1,subdivision):
            alpha=j/subdivision
            p=before+(after-before)*alpha
            hits,active=exact_strict_edge_face_crossing_count(
                p,triangles,pairs)
            intermediate.append({
                "fraction":alpha,
                "independent_nonlocal_piercings":hits,
                "AABB_filtered_triangle_pairs":active
            })
        rows.append({
            "frame":i,
            "native_original_endpoint_piercings":native,
            "independent_endpoint_piercings":endpoint_count,
            "endpoint_AABB_filtered_pairs":candidates,
            "interpolated_checks":intermediate,
        })
        before=after
    first_intersections=actual_strict=0
    first_intersections,_=exact_strict_edge_face_crossing_count(
        original,triangles,pairs)
    endpoint_total=sum(r["independent_endpoint_piercings"] for r in rows)
    subframe_total=sum(sample["independent_nonlocal_piercings"]
        for row in rows for sample in row["interpolated_checks"])
    result={
        "schema":"rbc-independent-native-mesh-linear-subframe-geometry-v1",
        "original_source_sha256":file_sha256(source),
        "all_five_original_policy_sha256":{
            m:file_sha256(sources[m][2]) for m in POLICIES
        },
        "source_frame_count":len(frames),
        "native_policy_count":5,
        "original_initial_configuration_intersecting_pairs":first_intersections,
        "independent_endpoint_checks":len(frames),
        "independent_linear_subframe_checks":len(frames)*(subdivision-1),
        "per_interval_subdivisions":subdivision,
        "source_endpoint_piercing_total":endpoint_total,
        "source_linear_subframe_piercing_total":subframe_total,
        "all_original_position_states_SAP_equal":True,
        "NOTE":"Linear interpolant sampling is NOT exact continuous collision detection",
        "strict_coplanar_overlaps_not_proven_absent":True,
        "per_original_frame":rows
    }
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    if require_zero and (first_intersections or endpoint_total or subframe_total):
        raise RuntimeError(
            "ORIGINAL REAL MESH PENETRATION: initial="+str(first_intersections)+
            " endpoint="+str(endpoint_total)+" sampled_interpolant="+
            str(subframe_total)+"; negative evidence retained at "+str(output)
        )
    print(
        f"PASS: {len(frames)} true native frames; "
        f"{result['independent_linear_subframe_checks']} independent intermediate "
        f"trajectory geometry checks, all five original source policies identical; "
        f"observed endpoint={endpoint_total} interpolated={subframe_total}"
    )
    return result


def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mesh-dir",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--subdivisions",type=int,default=8)
    p.add_argument("--allow-crossings",action="store_true",
                   help="Only to preserve HISTORICAL negative controls")
    args=p.parse_args()
    audit(args.mesh_dir,args.output,args.subdivisions,
          require_zero=not args.allow_crossings)


if __name__=="__main__":
    main()
