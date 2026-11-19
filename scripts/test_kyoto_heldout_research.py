#!/usr/bin/env python3
"""Independent audit for new Kyoto geometry/topology and coupled solver evidence.

Never use success-colored counts as proof of full physical fidelity. Verify
all raw native evidence, source SHA-256, full AABB pair hash/count parity,
different first-frame geometry across variation seeds and actual mesh
vertex/triangle/contact parity in the existing coupled solver.
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import HEADER, snapshot_metadata, sha256
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score


def ensure(condition, message):
    if not condition:
        raise ValueError(message)


def inspect(root: Path):
    source = root / "HELDOUT_RESULTS.json"
    data = json.loads(source.read_text(encoding="utf-8"))
    ensure(data["experiment"] ==
           "Kyoto independent geometry and topology plus coupled solver audit",
           "unknown or mislabeled research experiment")
    ensure(data["frozen_v4_unchanged"] is True and
           data["historical_phase18_inputs_untouched"] is True,
           "manifest does not preserve historical frozen controls")
    trajectories = data["material_replays"]
    ensure(len(trajectories) == data["new_varied_material_scenes"],
           "missing/new heldout material output")
    first_frames = defaultdict(set)
    for item in trajectories:
        scene = Path(item["snapshot"])
        replay = Path(item["replay"])
        ensure(scene.is_file() and sha256(scene) == item["snapshot_sha256"],
               "source snapshot digest mismatch: " + str(scene))
        ensure(replay.is_file() and sha256(replay) == item["replay_sha256"],
               "native replay digest mismatch: " + str(replay))
        meta = snapshot_metadata(scene)
        ensure(meta["n"] == item["actual_n"] and
               meta["seed"] == item["motion"], "binary scene header mismatch")
        with scene.open("rb") as input_file:
            input_file.seek(HEADER.size)
            first = input_file.read(meta["n"] * 24)
        ensure(hashlib.sha256(first).hexdigest() ==
               item["first_frame_sha256"], "first-frame source tampered")
        key = (item["material"], item["regime"], item["budget"])
        ensure(item["first_frame_sha256"] not in first_frames[key],
               "allegedly independent geometry shares identical first frame")
        first_frames[key].add(item["first_frame_sha256"])
        scores = score(replay, skip=0)
        observed = scores["methods"]
        ensure(set(observed) == set(item["methods_actually_executed"]),
               "different method set from receipt")
        ensure({"sap_rebuild", "global_guarded_exact",
                "guard_sap_bounded_trial", "online_cost_aware"}.issubset(observed),
               "original policy comparator missing")
        times = (observed["global_guarded_exact"]["mean_ms"],
                 observed["guard_sap_bounded_trial"]["mean_ms"])
        # Manifest score excludes warmup, so only framewise parity is
        # independently rechecked here; never substitute untrimmed ratio.
        ensure(all(t > 0 and math.isfinite(t) for t in times),
               "nonfinite query time")
        ensure(item["guard_over_frozen_v4"] > 0 and
               math.isfinite(item["guard_over_frozen_v4"]),
               "nonsense frozen V4 time")
    ensure(bool(first_frames),"no new independent mesh snapshots evaluated")
    for variation in data["independent_geometry_topology_seeds_used"]:
        ensure(variation["geometry"] > 0 and variation["topology"] > 0,
               "only old phase-offset seeds used")
    seen_seeds = {(x["geometry"], x["topology"])
                  for x in data["independent_geometry_topology_seeds_used"]}
    ensure(len(seen_seeds) ==
           len(data["independent_geometry_topology_seeds_used"]),
           "duplicate geometry/topology seed pair")
    total_coupled = 0
    distinct_coupled_geometry = defaultdict(set)
    distinct_coupled_topology = defaultdict(set)
    actual_contact_variants = set()
    for scenario in data["coupled_solver_runs"]:
        csv_path = Path(scenario["source"])
        directory = csv_path.parent
        ensure(csv_path.is_file() and sha256(csv_path) == scenario["sha256"],
               "original coupled contact CSV SHA mismatch")
        with csv_path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        names = {"sap_rebuild", "global_guarded", "guard_sap_bounded_trial"}
        ensure({r["policy"] for r in rows} == names,
               "incomplete coupled solver cohort")
        n_frames = int(scenario["frames"])
        ensure(len(rows) == n_frames*len(names),
               "missing coupled physically solved frames")
        for name in names:
            subset = [r for r in rows if r["policy"] == name]
            ensure([int(r["frame"]) for r in subset] == list(range(n_frames)),
                   "wrong coupled solver frame indices")
        if (sum(int(r["precise_tests"]) for r in rows if r["policy"]=="sap_rebuild")>0 and
            sum(int(r["self_contacts"])+int(r["sphere_contacts"])
                for r in rows if r["policy"]=="sap_rebuild")>0):
            actual_contact_variants.add((scenario["geometry_seed"],scenario["topology_seed"]))
        for r in rows:
            ensure(int(r["pinned_vertices"]) in (4,6),
                   "unexpected held-out physical cloth anchor count")
            if r["policy"]=="guard_sap_bounded_trial":
                ensure(int(r["frozen_guard_queries"])+
                       int(r["frozen_sap_queries"])==5,
                       "original frozen V4 selection missing solver iterations")
                ensure(r["last_broadphase_action"] in (
                       "global_guarded_exact","sap_persistent_coherent"),
                       "invalid measured physical V4 backend choice")
            else:
                ensure(all(int(r[k])==0 for k in (
                       "frozen_guard_queries","frozen_sap_queries",
                       "frozen_trial_queries","frozen_switches")),
                       "non-V4 policy contains synthetic V4 decision telemetry")
            ensure(int(r["geometry_seed"]) == scenario["geometry_seed"] and
                   int(r["topology_seed"]) == scenario["topology_seed"],
                   "coupled physical geometry/topology seed provenance missing")
            ensure(int(r["oracle_misses"]) == 0,
                   "physical solver's broad-phase reported missing candidate")
            ensure(float(r["max_position_difference_from_sap"]) <= 1e-7,
                   "divergent contact solver positions")
            for key in ("broad_ms", "narrow_ms", "step_ms",
                        "rms_relative_edge_strain", "max_edge_stretch"):
                ensure(math.isfinite(float(r[key])) and float(r[key]) >= 0,
                       "invalid physical solver source metrics")
        streams = {}
        for name in names:
            path = directory / (name + ".jsonl")
            ensure(sha256(path) == scenario["mesh_stream_sha256"][name],
                   "physical mesh JSONL SHA mismatch")
            with path.open(encoding="utf-8") as stream:
                lines = [json.loads(line) for line in stream if line.strip()]
            ensure(len(lines) == n_frames+1,
                   "missing real physical mesh frame")
            header = lines[0]
            ensure(header["type"]=="mesh_header" and header["policy"]==name,
                   "wrong mesh/solver identity")
            ensure(all(f["type"]=="frame" and f["frame"]==i
                       for i,f in enumerate(lines[1:])),
                   "wrong physical mesh chronology")
            streams[name] = lines
        truth = streams["sap_rebuild"]
        ensure(truth[0]["geometry_seed"] == scenario["geometry_seed"] and
               truth[0]["topology_seed"] == scenario["topology_seed"],
               "mesh source header lacks independent topology provenance")
        ensure(truth[0]["pinned_vertices"] in (4,6),
               "mesh source does not serialize valid actual anchor count")
        canonical = lambda obj: json.dumps(
            obj, separators=(",", ":")).encode("utf-8")
        initial = hashlib.sha256(canonical(truth[0]["rest_positions"])).hexdigest()
        topology = hashlib.sha256(canonical(truth[0]["triangles"])).hexdigest()
        ensure(initial == scenario["source_rest_positions_sha256"] and
               topology == scenario["source_triangle_topology_sha256"],
               "coupled source mesh vertex or adjacency fingerprint altered")
        distinct_coupled_geometry[scenario["scenario"]].add(initial)
        distinct_coupled_topology[scenario["scenario"]].add(topology)
        for name in names - {"sap_rebuild"}:
            other = streams[name]
            ensure(truth[0]["triangles"] == other[0]["triangles"] and
                   truth[0]["triangle_sheets"] == other[0]["triangle_sheets"],
                   "changed physical triangle topology in different policies")
            for i in range(1, n_frames+1):
                a,b = truth[i]["positions"], other[i]["positions"]
                ensure(len(a)==len(b),"changed physical particle count")
                worst = max((max(abs(u-v) for u,v in zip(x,y))
                             for x,y in zip(a,b)), default=0.)
                ensure(worst <= 1e-7,
                       "visual differs between exact-candidate physics policies")
        total_coupled += n_frames
    ensure(total_coupled>0,"no coupled-contact simulations")
    n_variants = len(data["independent_geometry_topology_seeds_used"])
    assert all(len(x["pinned_vertices"]) == 1
               for x in data["coupled_solver_runs"]), (
                   "same seeded policy cohort unexpectedly changes pin counts"
               )
    pinned_by_topo = {}
    for x in data["coupled_solver_runs"]:
        pinned_by_topo.setdefault(x["topology_seed"], x["pinned_vertices"][0])
    if n_variants>=2:
        ensure(len(set(pinned_by_topo.values()))>=2,
               "independent topological pin-constraint variant was not tested")
    ensure(data.get("actual_physical_contact_coverage_enforced") is True,
           "actual physical contact gate not recorded")
    ensure(len(actual_contact_variants)==n_variants,
           "one or more independent coupled physical meshes generated no real contact")
    for kind in {item["scenario"] for item in data["coupled_solver_runs"]}:
        ensure(len(distinct_coupled_geometry[kind]) == n_variants,
               "coupled physical geometry failed to vary with independent seeds")
        ensure(len(distinct_coupled_topology[kind]) == n_variants,
               "coupled physical triangle topology failed to vary with independent seeds")
    print("PASS: independently varied first-frame geometry AND coupled solver mesh adjacency, all original")
    print("      full-pair policy outputs, receipts, and coupled mesh/contact parity")
    print("      scenes:",len(trajectories),"coupled scenario-frames:",total_coupled)
    print("      not proof of continuous collision, mesh penetration, or speedup")


if __name__=="__main__":
    if len(sys.argv)!=2:
        raise SystemExit("usage: test_kyoto_heldout_research.py OUTPUT_DIR")
    inspect(Path(sys.argv[1]))
