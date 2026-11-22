#!/usr/bin/env python3
"""Export source-backed moving triangle surfaces into ParaView VTK time series.

Input is ONLY the source-derived trajectory.csv from C++ kyoto_motion_export.
The moving triangle is a kinematic analytical test, not a physical solver.
The point scalar is longitudinal displacement, NOT strain or stress.
"""
from __future__ import annotations
import argparse
import csv
from collections import defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET

def load(path):
    frames=defaultdict(lambda:defaultdict(dict))
    with path.open(newline="",encoding="utf-8") as f:
        for r in csv.DictReader(f):
            frame=int(r["frame"])
            obj=int(r["object"])
            vid=int(r["vertex"])
            xyz=tuple(float(r[k]) for k in ("x","y","z"))
            frames[frame][obj][vid]=xyz
    return frames

def vtk_file(path,frame,objects,initial):
    names=sorted(objects)
    if names!=[11,19]:
        raise ValueError(f"unknown scene objects: {names}")
    points=[]
    labels=[]
    displacements=[]
    for obj in names:
        if sorted(objects[obj])!=[0,1,2]:
            raise ValueError("triangle needs exactly three vertices")
        for vid in (0,1,2):
            p=objects[obj][vid]
            points.append(p)
            labels.append(obj)
            displacements.append(p[0]-initial[obj][vid][0])
    with path.open("w",encoding="utf-8") as out:
        out.write("# vtk DataFile Version 3.0\n")
        out.write(f"Kyoto kinematic cubic motion frame {frame}\n")
        out.write("ASCII\nDATASET POLYDATA\n")
        out.write("POINTS 6 double\n")
        for p in points:
            out.write("%.17g %.17g %.17g\n" % p)
        out.write("POLYGONS 2 8\n3 0 1 2\n3 3 4 5\n")
        out.write("POINT_DATA 6\n")
        out.write("SCALARS longitudinal_displacement double 1\n")
        out.write("LOOKUP_TABLE default\n")
        for d in displacements:
            out.write(f"{d:.17g}\n")
        out.write("SCALARS vertex_object_id int 1\n")
        out.write("LOOKUP_TABLE default\n")
        for label in labels:
            out.write(f"{label}\n")
        out.write("CELL_DATA 2\nSCALARS object_id int 1\n")
        out.write("LOOKUP_TABLE default\n")
        for label in names:
            out.write(f"{label}\n")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--data",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    frames=load(args.data/"trajectory.csv")
    if sorted(frames)!=list(range(121)):
        raise ValueError("expected 121 contiguous real exported frames")
    args.output.mkdir(parents=True,exist_ok=True)
    root=ET.Element("VTKFile",type="Collection",version="0.1",
                    byte_order="LittleEndian")
    coll=ET.SubElement(root,"Collection")
    for frame in sorted(frames):
        name=f"triangle_motion_{frame:04d}.vtk"
        vtk_file(args.output/name,frame,frames[frame],frames[0])
        ET.SubElement(coll,"DataSet",
                      timestep=f"{frame/120:.17g}",group="",part="0",
                      file=name)
    ET.indent(root,space="  ")
    ET.ElementTree(root).write(args.output/"motion.pvd",
                               encoding="utf-8",xml_declaration=True)
    print("PASS: 121 native-source VTK polygon frames + ParaView time series")

if __name__=="__main__":
    main()
