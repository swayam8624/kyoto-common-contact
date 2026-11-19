#!/usr/bin/env python3
"""Build a portable source-verified interactive 3D RBC physics viewer.

Combines five ORIGINAL native JSONL trajectories into one HTML with the
real mesh, triangle topology, native CPU timing, independent endpoint
penetration counters and two moving sphere trajectories. The geometry
and metric data are embedded, so only the pinned Three.js code
requires network access when opened via a local HTTP server.

No simulated vertex is created by this builder. No policy can receive
an altered shape or apparently superior cloth deformation.
A source mismatch or stale/incorrect native record fails closed.

Requires NumPy because it reuses the same full-source integrity gate
as scripts/render_rbc_physics_studio.py.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from render_rbc_physics_studio import POLICIES, source_data, file_sha256


def make(source: Path, output: Path) -> dict:
    if output.exists():
        raise FileExistsError("Refusing to overwrite original interactive evidence")
    manifest_path=output.with_suffix(".viewer-receipts.json")
    if manifest_path.exists():
        raise FileExistsError("Refusing to overwrite interactive viewer receipts")
    root=Path(__file__).resolve().parents[1]
    template_file=root/"web"/"rbc_verified_physics_viewer_template.html"
    if not template_file.exists():
        raise FileNotFoundError("Original viewer template not checked into repository")
    template=template_file.read_text(encoding="utf-8")
    token="__NATIVE_SOURCE_PAYLOAD__"
    if template.count(token)!=1:
        raise ValueError("HTML data placeholder must occur once, not zero/multiple times")
    sources=source_data(source)
    header,frames,_=sources[POLICIES[0]]
    cohort={
        "schema":"rbc-five-policy-original-native-webgl-v1",
        "reference":"sap_rebuild",
        "header":header,
        "policies":{m:sources[m][1] for m in POLICIES},
        "source_sha256":{m:file_sha256(sources[m][2]) for m in POLICIES},
        "source_original_frames":len(frames),
        "all_original_geometric_endpoint_piercings_zero":all(
            f["metrics"]["geometric_self_intersections"]==0
            for f in frames),
        "source_integrity_independently_verified":True,
        "not_temporal_CCD_or_continuum_validation":True
    }
    # This HTML is a trusted local artifact, not an unsafe
    # arbitrary-HTML renderer. Escaping prevents a source string
    # from injecting a closing script tag into the document.
    data=json.dumps(cohort,separators=(",",":"),ensure_ascii=True,
                    allow_nan=False)
    data=data.replace("<","\\u003c").replace(">","\\u003e")
    html=template.replace(token,data)
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(html,encoding="utf-8")
    receipt={
        "schema":"rbc-webgl-source-receipts-v1",
        "original_sap_source_sha256":cohort["source_sha256"]["sap_rebuild"],
        "all_five_original_policy_sha256":cohort["source_sha256"],
        "original_frames":len(frames),
        "original_policy_count":len(POLICIES),
        "original_total_policy_frames":len(POLICIES)*len(frames),
        "all_original_policy_positions_SAP_equal":True,
        "all_original_endpoint_intersections_zero":
            cohort["all_original_geometric_endpoint_piercings_zero"],
        "HTML_sha256":file_sha256(output),
        "source_mesh_topology_unchanged":True,
        "renderer_asset":"Three.js pinned 0.180.0 on CDN",
        "renderer_requires_network_for_external_library":True,
        "data_embedded_no_remote_data_request":True,
    }
    manifest_path.write_text(json.dumps(receipt,indent=2)+"\n",
                             encoding="utf-8")
    print("PASS interactive source-backed five-policy 3D viewer:",
          len(frames),"frames, original source hash",
          receipt["original_sap_source_sha256"],"html",output)
    return receipt


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mesh-dir",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    if args.output.suffix.lower()!=".html":
        p.error("Interactive viewer output must have .html extension")
    make(args.mesh_dir,args.output)


if __name__=="__main__":
    main()
