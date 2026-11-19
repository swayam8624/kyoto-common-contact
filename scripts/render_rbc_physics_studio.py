#!/usr/bin/env python3
"""Scientific studio renderer of actual Kyoto five-policy physical meshes.

Unlike the early 2D polygon/wireframe demonstration, this creates a real
SOFTWARE Z-BUFFER on original 3D vertex positions and moving sphere centers.
Occlusion is geometry-derived. Facet shading and visual fabric color are
presentation only; no particle positions, crossings or performance values
are synthesized.

Reads the same five independently advancing source JSONLs produced by
ccp_rbc_coupled_tournament; checks every frame's original SAP-referenced
particle-state agreement and actual endpoint penetration counters.

A CLEAN badge is shown ONLY if the independent original triangle
intersection counter was zero on all frames. Historical intersecting
fixtures instead receive an explicit red WARNING.

Requires Pillow + NumPy; this post-compute visualization is not charged
to any measured solver performance. No continuous CCD claim.

Example:
  python3 scripts/render_rbc_physics_studio.py \
    --mesh-dir output/rbc-rounded-clean-80/g73013-mesh \
    --output output/rbc-rounded-clean-80/five-physical-policies.gif \
    --stride 3 --fps 12
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np

POLICIES = ("sap_rebuild", "global_guard", "frozen_v4",
            "rbc_indexed", "rbc_flat")
TITLES = ("01 / Rebuilt SAP", "02 / Global Guard", "03 / Frozen V4",
          "04 / RBC node index", "05 / RBC flat frontier")
FABRIC_RGB = np.array([[57,154,176],[219,145,104]], dtype=np.float64)


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1<<20), b""):
            h.update(block)
    return h.hexdigest()


def source_data(root: Path) -> dict:
    sources = {}
    for name in POLICIES:
        path = root/(name + ".jsonl")
        if not path.is_file():
            raise FileNotFoundError("Missing original native mesh: " + str(path))
        raw = [json.loads(line) for line in
               path.read_text(encoding="utf-8").splitlines()]
        if len(raw) < 3 or raw[0].get("type") != "mesh_header":
            raise ValueError("Missing real native source mesh header")
        header, frames = raw[0], raw[1:]
        if header.get("schema") != 2 or header.get("policy") != name:
            raise ValueError("Original native geometry schema/policy mismatch")
        if [f.get("frame") for f in frames] != list(range(len(frames))):
            raise ValueError("Missing or reordered real source frames")
        if not header.get("triangles"):
            raise ValueError("Empty original source mesh triangles")
        vertices = len(header["initial_positions"])
        if any(len(f.get("positions", [])) != vertices for f in frames):
            raise ValueError("Native source mesh changed vertex cardinality")
        triangles = np.asarray(header["triangles"], dtype=int)
        if triangles.ndim != 2 or triangles.shape[1] != 3 or \
                triangles.min() < 0 or triangles.max() >= vertices:
            raise ValueError("Invalid original source triangle topology")
        if len(header["triangle_sheets"]) != len(triangles):
            raise ValueError("Original cloth sheet identity mismatch")
        for f in frames:
            metric = f.get("metrics", {})
            if metric.get("source_max_position_error_sap", float("inf")) > 1e-7:
                raise ValueError("Original physics diverged from independent SAP")
            if not isinstance(metric.get("geometric_self_intersections"), int) or \
                    metric["geometric_self_intersections"] < 0:
                raise ValueError("Missing independently audited geometric piercing count")
            if not isinstance(metric.get("self_contacts"), int):
                raise ValueError("Missing real solver contact count")
            if len(f.get("spheres", [])) != 2:
                raise ValueError("Missing source sphere centers/radii")
            if not np.isfinite(np.asarray(f["positions"], dtype=float)).all():
                raise ValueError("Nonfinite original source mesh")
        sources[name] = (header, frames, path)
    canonical_header, reference_frames, _ = sources[POLICIES[0]]
    for name in POLICIES[1:]:
        header, frames, _ = sources[name]
        for field in ("triangles", "triangle_sheets", "initial_positions"):
            if header[field] != canonical_header[field]:
                raise ValueError("Five policies have different original mesh or topology")
        if len(frames) != len(reference_frames):
            raise ValueError("Native experiment has missing policy frames")
        for a, b in zip(frames, reference_frames):
            if a["metrics"]["self_contacts"] != b["metrics"]["self_contacts"] or \
                    a["metrics"]["geometric_self_intersections"] != \
                    b["metrics"]["geometric_self_intersections"]:
                raise ValueError("Different real physical correction or piercing counters")
            pa = np.asarray(a["positions"], dtype=float)
            pb = np.asarray(b["positions"], dtype=float)
            if np.max(np.abs(pa-pb)) > 1e-7:
                raise ValueError("Full source physics positions differ from SAP")
    return sources


def font_of(size: int, bold: bool = False):
    from PIL import ImageFont
    names = ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
             if bold else
             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
             "DejaVuSans.ttf")
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


class OrthographicCamera:
    """Exactly one fitted projection over the complete shared source run."""

    def __init__(self, frames: list, width: int, height: int,
                 focus_vertices: int | None = None):
        up = np.array([0., 0., 1.])
        observer = np.array([3.2, -5.0, 3.8])
        observer /= np.linalg.norm(observer)
        self.forward = -observer
        self.right = np.cross(self.forward, up)
        self.right /= np.linalg.norm(self.right)
        self.up = np.cross(self.right, self.forward)
        self.up /= np.linalg.norm(self.up)
        all_points = np.concatenate([
            np.asarray(f["positions"][:focus_vertices]
                       if focus_vertices is not None else f["positions"],
                       dtype=float) for f in frames
        ], axis=0)
        self.target = (all_points.min(axis=0)+all_points.max(axis=0))/2
        x = (all_points-self.target)@self.right
        y = (all_points-self.target)@self.up
        xmin, xmax = float(x.min()), float(x.max())
        ymin, ymax = float(y.min()), float(y.max())
        self.scale = min((width-55)/max(1e-6, xmax-xmin),
                         (height-118)/max(1e-6, ymax-ymin))
        self.ox = width/2 - (xmin+xmax)*self.scale/2
        self.oy = height/2 + (ymin+ymax)*self.scale/2 + 17
        light = np.array([.15,-.40,1.])
        self.light = light/np.linalg.norm(light)

    def project(self, points):
        v = np.asarray(points, dtype=float) - self.target
        x = v@self.right*self.scale+self.ox
        y = -(v@self.up)*self.scale+self.oy
        z = v@self.forward
        return np.stack((x,y,z),axis=-1)


def render_mesh(rgb: np.ndarray, depth: np.ndarray, points,
                triangles: np.ndarray, sheets: np.ndarray,
                cam: OrthographicCamera) -> None:
    """Depth-tested 3D triangles; no arbitrary painter's ordering."""
    height,width = depth.shape
    xyz = np.asarray(points,dtype=float)
    projected = cam.project(xyz)
    for i, triangle in enumerate(triangles):
        a,b,c = projected[triangle]
        x0=max(0,int(np.floor(min(a[0],b[0],c[0]))))
        x1=min(width,int(np.ceil(max(a[0],b[0],c[0]))))
        y0=max(0,int(np.floor(min(a[1],b[1],c[1]))))
        y1=min(height,int(np.ceil(max(a[1],b[1],c[1]))))
        if x0>=x1 or y0>=y1:
            continue
        v1=xyz[triangle[1]]-xyz[triangle[0]]
        v2=xyz[triangle[2]]-xyz[triangle[0]]
        normal=np.cross(v1,v2)
        n=np.linalg.norm(normal)
        if n<1e-12:
            continue
        normal /= n
        intensity=.55+.47*abs(float(normal@cam.light))
        color=np.clip(FABRIC_RGB[sheets[i]%len(FABRIC_RGB)]*intensity,0,255)
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-9:
            continue
        yy,xx=np.mgrid[y0:y1,x0:x1]
        xx=xx+.5
        yy=yy+.5
        w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
        w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den
        w2=1-w0-w1
        inside=(w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)
        distances=w0*a[2]+w1*b[2]+w2*c[2]
        local_depth=depth[y0:y1,x0:x1]
        visible=inside&(distances<local_depth)
        if not visible.any():
            continue
        local_depth[visible]=distances[visible]
        surface=rgb[y0:y1,x0:x1]
        # Subtle visual fabric variation only; NO modified geometry.
        ribs=1.+.018*np.sin(yy*.30+xx*.12)
        for channel in range(3):
            surface[:,:,channel][visible]=np.clip(
                color[channel]*ribs[visible],0,255).astype(np.uint8)


def render_spheres(rgb, depth, spheres, cam: OrthographicCamera):
    """Real sphere centers/radii with the same 3D depth test."""
    height,width=depth.shape
    for sphere in spheres:
        center=sphere["center"]
        radius=float(sphere["radius"])
        cx,cy,cz=cam.project([center])[0]
        projected_radius=radius*cam.scale
        x0=max(0,int(cx-projected_radius))
        x1=min(width,int(cx+projected_radius)+1)
        y0=max(0,int(cy-projected_radius))
        y1=min(height,int(cy+projected_radius)+1)
        if x0>=x1 or y0>=y1:
            continue
        yy,xx=np.mgrid[y0:y1,x0:x1]
        x=(xx+.5-cx)/cam.scale
        y=-(yy+.5-cy)/cam.scale
        dist2=x*x+y*y
        inside=dist2<radius*radius
        z=np.sqrt(np.maximum(0.,radius*radius-dist2))
        local_depth=depth[y0:y1,x0:x1]
        visible=inside&(cz-z<local_depth)
        local_depth[visible]=cz-z[visible]
        shade=np.clip(.4+.65*(.3*x/radius+.38*y/radius+
                             .65*z/radius),.25,1.)
        surface=rgb[y0:y1,x0:x1]
        for channel,base in enumerate((126,140,172)):
            surface[:,:,channel][visible]=np.clip(
                base*shade[visible],0,255).astype(np.uint8)


def draw_panel(source, cam, frame: int, policy_number: int,
               width: int, height: int):
    from PIL import Image, ImageDraw
    yy=np.arange(height)[:,None]
    pixels=np.zeros((height,width,3),dtype=np.uint8)
    for channel,(top,bottom) in enumerate(((24,35),(34,45),(42,59))):
        pixels[:,:,channel]=(top+(bottom-top)*yy/height).astype(np.uint8)
    depths=np.full((height,width),float("inf"))
    head,frames,_=source
    current=frames[frame]
    triangles=np.asarray(head["triangles"],dtype=int)
    sheets=np.asarray(head["triangle_sheets"],dtype=int)
    render_spheres(pixels,depths,current["spheres"],cam)
    render_mesh(pixels,depths,current["positions"],triangles,sheets,cam)
    image=Image.fromarray(pixels,"RGB")
    brush=ImageDraw.Draw(image)
    brush.rounded_rectangle((5,5,width-6,height-6),radius=9,
                            outline=(83,95,110),width=2)
    brush.text((19,15),TITLES[policy_number],font=font_of(16,True),
               fill=(237,242,249))
    brush.text((width-105,18),f"{frame+1:03d}/{len(frames):03d}",
               font=font_of(11),fill=(165,179,192))
    metric=current["metrics"]
    crossings=int(metric["geometric_self_intersections"])
    status=("ZERO ENDPOINT PIERCINGS" if crossings==0
            else f"WARNING: {crossings} PENETRATIONS")
    color=(122,224,181) if crossings==0 else (255,110,93)
    brush.rounded_rectangle((16,height-72,width-16,height-13),
                            radius=7,fill=(15,22,31))
    brush.text((26,height-65),status,font=font_of(11,True),fill=color)
    brush.text((26,height-44),
               (f"{metric['self_contacts']:,} contact corrections  "
                f"| {metric['step_ms']:.2f} ms step  "
                f"| {metric['broad_ms']:.2f} ms broad"),
               font=font_of(11),fill=(207,218,228))
    return image


def draw_original_fold_inspector(source, cam, frame: int,
                                 width: int, height: int):
    """Sixth panel: REAL first sheet, fitted independently for inspection.

    It is a CLOSE-UP, not a sixth algorithm. The second sheet and
    obstacles are intentionally hidden in this panel to reveal
    the true U-fold cross section. No geometry is modified.
    """
    from PIL import Image, ImageDraw
    pixels=np.zeros((height,width,3),dtype=np.uint8)
    pixels[:,:,:]=[23,31,40]
    depth=np.full((height,width),float("inf"))
    head,frames,_=source
    sample=frames[frame]
    tri=np.asarray(head["triangles"],dtype=int)
    sheets=np.asarray(head["triangle_sheets"],dtype=int)
    selected=tri[sheets==0]
    render_mesh(pixels,depth,sample["positions"],selected,
                np.zeros(len(selected),dtype=int),cam)
    image=Image.fromarray(pixels,"RGB")
    d=ImageDraw.Draw(image)
    d.rounded_rectangle((5,5,width-6,height-6),radius=9,
                        outline=(83,95,110),width=2)
    d.text((18,15),"SOURCE FOLD / MAGNIFIED",font=font_of(15,True),
           fill=(240,245,250))
    d.text((width-112,18),f"{frame+1:03d}/{len(frames):03d}",
           font=font_of(11),fill=(168,185,199))
    metric=sample["metrics"]
    bad=int(metric["geometric_self_intersections"])
    d.rounded_rectangle((16,height-74,width-16,height-13),radius=7,
                        fill=(15,22,31))
    d.text((26,height-66),
           ("TRUE SOURCE: ZERO CURRENT PIERCINGS" if bad==0 else
            f"INVALID SOURCE: {bad} PIERCINGS"),
           font=font_of(11,True),
           fill=(128,228,189) if bad==0 else (255,115,96))
    d.text((26,height-43),
           "Sheet A only | obstacles & second sheet hidden",
           font=font_of(11),fill=(194,207,222))
    return image


def render(sources: dict, output: Path, stride: int,
           fps: int, width: int = 440, height: int = 330):
    from PIL import Image, ImageDraw
    if output.exists() or output.with_suffix(".png").exists() or \
            output.with_suffix(".json").exists():
        raise FileExistsError("Do not overwrite original render/evidence")
    if output.suffix.lower()!=".gif":
        raise ValueError("Native comparison requires GIF output")
    head, frames, _=sources[POLICIES[0]]
    times=list(range(0,len(frames),stride))
    if times[-1]!=len(frames)-1:
        times.append(len(frames)-1)
    cam=OrthographicCamera(frames,width,height)
    folded_vertices=1+max(
        max(tri) for tri,sid in
        zip(head["triangles"],head["triangle_sheets"]) if sid==0)
    close_cam=OrthographicCamera(frames,width,height,
                                 focus_vertices=folded_vertices)
    images=[]
    for t in times:
        grid=Image.new("RGB",(3*width,2*height),(20,27,35))
        for i,name in enumerate(POLICIES):
            panel=draw_panel(sources[name],cam,t,i,width,height)
            grid.paste(panel,(i%3*width,i//3*height))
        # Six policy-grid cells: FIVE identical-world method views,
        # plus one magnified source fold inspection perspective.
        # Magnification never changes particle coordinates.
        detail=draw_original_fold_inspector(sources[POLICIES[0]],
                                            close_cam,t,width,height)
        grid.paste(detail,(2*width,height))
        images.append(grid)
    output.parent.mkdir(parents=True,exist_ok=True)
    images[0].save(output,save_all=True,append_images=images[1:],
                   duration=max(40,int(1000/fps)),loop=0,optimize=False)
    preview=output.with_suffix(".png")
    images[min(3,len(images)-1)].save(preview)
    receipts={
        "schema":"rbc-true-geometry-software-depth-test-v2",
        "source_kind":"real-original-solver-mesh-JSONL",
        "source_sha256":{name:file_sha256(sources[name][2])
                         for name in POLICIES},
        "policy_count":5,
        "source_original_frames":len(frames),
        "gif_frames":len(times),
        "source_frame_indices":times,
        "all_source_policy_states_SAP_equal":True,
        "all_source_frames_have_independent_piercing_audit":True,
        "all_source_frames_endpoint_piercing_free":all(
            f["metrics"]["geometric_self_intersections"]==0 for f in frames),
        "zero_nonlocal_self_intersection_is_not_3d_continuous_CCD":True,
        "fixed_camera_and_scale_for_all_frames_policies":True,
        "true_software_Z_buffer":True,
        "sixth_panel_original_source_fold_closeup":True,
        "sixth_panel_is_not_sixth_algorithm":True,
        "second_sheet_and_obstacles_hidden_only_in_closeup":True,
        "native_source_geometry_not_edited":True,
        "gif_sha256":file_sha256(output),
        "preview_png_sha256":file_sha256(preview),
    }
    output.with_suffix(".json").write_text(json.dumps(receipts,indent=2)+"\n")
    print("PASS: original solver geometry in 5 depth-tested synchronized "
          "panels, source SHA receipts, "+str(len(times))+" frames")
    return receipts


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mesh-dir",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--stride",type=int,default=3)
    p.add_argument("--fps",type=int,default=12)
    args=p.parse_args()
    if not 1<=args.stride<=20 or not 1<=args.fps<=30:
        p.error("Frame stride and FPS outside supported ranges")
    results=render(source_data(args.mesh_dir),args.output,
                   args.stride,args.fps)
    print(results["gif_sha256"])


if __name__ == "__main__":
    main()
