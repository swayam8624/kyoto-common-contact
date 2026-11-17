#!/usr/bin/env python3
"""Fast regression tests for honest visual and external-data evidence pipelines."""
from __future__ import annotations

import csv
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT))
from benchmarks.import_mesh_sequence import convert,HEADER
from benchmarks.score_replay import score
from benchmarks.quality_report import analyze
from scripts.render_kyoto_visuals import render_aabb,render_solver,MATERIALS,REGIMES,DEFAULT_METHODS

class VisualDataTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="kyoto-visual-")
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)

    def test_obj_imports_real_triangle_bounds_and_rejects_topology_changes(self):
        src=self.root/"objs";src.mkdir()
        mesh="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
        (src/"000.obj").write_text(mesh,encoding="utf-8")
        (src/"001.obj").write_text(mesh.replace("v 1 0 0","v 1 0 0.5"),encoding="utf-8")
        dst=self.root/"triangles.kmcb"
        result=convert(src,dst,padding=.1)
        self.assertEqual(result["triangle_count"],1)
        self.assertEqual(result["frames"],2)
        with dst.open("rb") as f:
            magic,n,frames,seed,mat=HEADER.unpack(f.read(HEADER.size))
            values=struct.unpack("<6f",f.read(24))
        self.assertEqual((magic,n,frames,mat),(b"KYOTOM01",1,2,0))
        self.assertAlmostEqual(values[0],-.1,places=6)
        (src/"001.obj").write_text(mesh.replace("f 1 2 3","f 1 3 2"),encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"topological change"):
            convert(src,self.root/"bad.kmcb")

    @staticmethod
    def metrics():
        return {
            "step_ms":2.,"broad_ms":.3,"narrow_ms":.5,
            "candidate_pairs":3,"self_contacts":1,"self_crossing_responses":0,
            "sphere_contacts":1,"precise_tests":10,
            "max_position_difference_from_SAP":0.,
            "rms_relative_edge_strain":.01,
            "near_degenerate_triangles":0,"velocity_clamps":0,
            "oracle_misses":0,"oracle_checked":True
        }

    def mesh_evidence(self,alter=False):
        source=self.root/"physical";source.mkdir()
        for method in ("sap_rebuild","global_guarded"):
            header={
                "type":"mesh_header","schema":1,"policy":method,
                "triangles":[[0,1,2]],"triangle_sheets":[0],
                "rest_positions":[[0.,0.,0.],[1.,0.,0.],[0.,1.,0.]]
            }
            with (source/f"{method}.jsonl").open("w",encoding="utf-8") as f:
                f.write(json.dumps(header)+"\n")
                for frame in range(3):
                    points=[[0.,0.,0.],[1.,0.,0.],[0.,1.,.01*frame]]
                    if alter and method!="sap_rebuild" and frame==1:
                        points[1][1]+=.3
                    f.write(json.dumps({
                        "type":"frame","frame":frame,"positions":points,
                        "spheres":[],"metrics":self.metrics()
                    })+"\n")
        return source

    def test_quality_and_solver_loop_reject_fabricated_policy_difference(self):
        root=self.mesh_evidence()
        data=analyze(root)
        self.assertTrue(data["all_trajectories_same_within_tolerance"])
        gif,_=render_solver(root,self.root/"mesh-loop",fps=4,stride=1)
        self.assertGreater(gif.stat().st_size,500)
        root=self.mesh_evidence(alter=True)
        with self.assertRaisesRegex(ValueError,"altered solver geometry"):
            analyze(root)
        with self.assertRaisesRegex(ValueError,"physics differs"):
            render_solver(root,self.root/"should-not-create")

    def fixtures(self,wrong=False):
        run=self.root/"run"
        (run/"snapshots").mkdir(parents=True)
        (run/"replays").mkdir()
        for material in MATERIALS:
            for regime in REGIMES:
                stem=f"actual-{material}-{regime}-n512-seed1001"
                path=run/"snapshots"/f"{stem}.kmcb"
                with path.open("wb") as f:
                    f.write(struct.pack("<8sIIII",b"KYOTOM01",4,8,1001,0))
                    for frame in range(8):
                        for i in range(4):
                            x=i*.15+frame*.01
                            f.write(struct.pack("<6f",x,0.,0.,x+.16,.1,.1))
                csv_file=run/"replays"/f"{stem}-rep1-evaluated.csv"
                with csv_file.open("w",newline="",encoding="utf-8") as fd:
                    w=csv.DictWriter(fd,fieldnames=(
                        "backend","frame","ms","pairs","pair_hash","misses"))
                    w.writeheader()
                    for method in DEFAULT_METHODS:
                        for frame in range(8):
                            h="BAD" if wrong and method==DEFAULT_METHODS[-1] and frame==2 else "FNV"
                            w.writerow({
                                "backend":method,"frame":frame,
                                "ms":.5+DEFAULT_METHODS.index(method)*.1,
                                "pairs":2,"pair_hash":h,"misses":0
                            })
        return run

    def test_aabb_grid_has_genuine_data_and_rejects_wrong_candidates(self):
        run=self.fixtures()
        report=score(next((run/"replays").glob("*.csv")),skip=1)
        self.assertTrue(report["methods"]["global_guarded_exact"]["full_frame_pair_parity"])
        gif,_=render_aabb(run,self.root/"all-material-loop",
                           DEFAULT_METHODS,512,1001,1,1,3)
        self.assertGreater(gif.stat().st_size,1000)
        run=self.fixtures(wrong=True)
        with self.assertRaisesRegex(ValueError,"INCORRECT"):
            score(next((run/"replays").glob("*.csv")),skip=1)
        with self.assertRaisesRegex(ValueError,"exact candidate pairs mismatch"):
            render_aabb(run,self.root/"bad-loop",DEFAULT_METHODS,
                        512,1001,1,1,3)

if __name__=="__main__":
    unittest.main()
