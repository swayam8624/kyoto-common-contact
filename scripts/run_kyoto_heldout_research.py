#!/usr/bin/env python3
"""Kyoto independent-geometry/topology challenge; frozen policies, new scenes.

This is NOT Phase 18 A/A repeated timings. NEW native mass-spring worlds
are generated with both independent initial node jitter and topology seeds.
Every candidate algorithm sees the same immutable Float32 KYOTOM01 stream
and is checked against SAP on every frame. Geometry and topology are
generated only once; no optimization, training, or threshold retuning.

Separately the existing contact-capable MultiCloth solver is exercised under
three original policies; solver geometry parity, contacts, strain and physical
mesh JSONL are generated. Its topology is still fixed, so it is not
represented as independently topologized cloth.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import (
    HEADER, random_order_seed, snapshot_metadata, sha256, check_ordered_replay
)
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score

# Separate these exact seeds from the inspected Phase 17/18 forcing-phase
# values. Different topology and geometry seeds are NOT substitutes for
# changing physical solver topology elsewhere.
HELDOUT = (
    {"motion": 91003, "geometry": 73013, "topology": 86011},
    {"motion": 91009, "geometry": 74017, "topology": 86019},
    {"motion": 91033, "geometry": 75029, "topology": 86029},
    {"motion": 91079, "geometry": 77041, "topology": 86069},
)
VALID_MATERIALS = (
    "cloth", "sponge", "cushion", "soft_ball", "spring", "mixed"
)
VALID_REGIMES = (
    "baseline", "compression", "shear", "impact", "rapid", "relax"
)


def run(cmd: list[str], logfile: Path, timeout: int) -> None:
    with logfile.open("x", encoding="utf-8") as output:
        output.write("COMMAND " + " ".join(cmd) + "\n")
        output.flush()
        try:
            status = subprocess.run(
                cmd, stdout=output, stderr=subprocess.STDOUT,
                check=False, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("Process exceeded bounded timeout: " +
                               str(logfile)) from error
    if status.returncode:
        raise RuntimeError("Nonzero subprocess exit: " + str(logfile))


def parse_coupled(path: Path, expected_frames: int, expected_scenario: str,
                  geometry_seed: int, topology_seed: int):
    import csv
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    methods = {"sap_rebuild", "global_guarded", "guard_sap_bounded_trial"}
    observed = {row["policy"] for row in rows}
    if observed != methods or len(rows) != expected_frames * len(methods):
        raise ValueError("Coupled simulation policy/frame coverage mismatch")
    for label in methods:
        entries = [r for r in rows if r["policy"] == label]
        if [int(r["frame"]) for r in entries] != list(range(expected_frames)):
            raise ValueError("Missing/duplicate coupled policy frame")
    for r in rows:
        if (r["scenario"] != expected_scenario or int(r["oracle_misses"]) or
            int(r["geometry_seed"]) != geometry_seed or
            int(r["topology_seed"]) != topology_seed):
            raise ValueError("Coupled physical source failed equality audit")
        if not all(math.isfinite(float(r[field])) and float(r[field]) >= 0
                   for field in (
                       "broad_ms", "narrow_ms", "step_ms",
                       "rms_relative_edge_strain", "max_edge_stretch",
                       "max_position_difference_from_sap"
                   )):
            raise ValueError("Invalid coupled physical metric")
        if float(r["max_position_difference_from_sap"]) > 1e-7:
            raise ValueError("Coupled solver states differ from SAP")
    return {
        "scenario": expected_scenario, "frames": expected_frames,
        "geometry_seed": geometry_seed, "topology_seed": topology_seed,
        "methods": sorted(methods),
        "source": str(path), "sha256": sha256(path),
        "full_geometry_agreement": True,
        "all_oracle_misses_zero": True,
        "contacts_by_method": {
            label: sum(int(r["self_contacts"])+int(r["sphere_contacts"])
                       for r in rows if r["policy"] == label)
            for label in sorted(methods)
        },
        "precise_tests_by_method": {
            label: sum(int(r["precise_tests"]) for r in rows
                       if r["policy"] == label)
            for label in sorted(methods)
        },
        "self_contacts_by_method": {
            label: sum(int(r["self_contacts"]) for r in rows
                       if r["policy"] == label)
            for label in sorted(methods)
        },
        "sphere_contacts_by_method": {
            label: sum(int(r["sphere_contacts"]) for r in rows
                       if r["policy"] == label)
            for label in sorted(methods)
        },
        "max_strain_by_method": {
            label: max(float(r["rms_relative_edge_strain"]) for r in rows
                       if r["policy"] == label)
            for label in sorted(methods)
        },
        "max_collapsed_triangles_by_method": {
            label: max(int(r["near_degenerate_triangles"]) for r in rows
                       if r["policy"] == label)
            for label in sorted(methods)
        },
        "step_time_mean_ms_by_method": {
            label: sum(float(r["step_ms"]) for r in rows
                       if r["policy"] == label) / expected_frames
            for label in sorted(methods)
        },
        "interpretation": (
            "One physical solver implementation stepped independently under "
            "three broad-phase policies. Reported step times include actual "
            "constraints and narrow-phase work, but sequential method order "
            "is NOT an unbiased runtime speedup study."
        )
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exporter", type=Path, required=True)
    p.add_argument("--replayer", type=Path, required=True)
    p.add_argument("--coupled", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--materials", nargs="+", default=["cloth", "sponge", "mixed"])
    p.add_argument("--regimes", nargs="+", default=["baseline", "rapid"])
    p.add_argument("--budgets", nargs="+", type=int, default=[128, 512])
    p.add_argument("--heldout-count", type=int, default=3)
    p.add_argument("--frames", type=int, default=36)
    p.add_argument("--warmup", type=int, default=8)
    p.add_argument("--coupled-frames", type=int, default=60)
    p.add_argument("--coupled-scenarios", nargs="+",
                   default=["free", "pull", "fold"])
    p.add_argument("--timeout", type=int, default=240)
    a = p.parse_args()

    if a.output.exists():
        p.error("Refusing to overwrite existing evidence output directory")
    if not 1 <= a.heldout_count <= len(HELDOUT):
        p.error("invalid number of independently varied held-out fixtures")
    if not 12 <= a.frames <= 300 or not 0 <= a.warmup < a.frames:
        p.error("frames and warmup outside supported range")
    if not 8 <= a.coupled_frames <= 120:
        p.error("coupled solver frames must be 8..120")
    if any(mat not in VALID_MATERIALS for mat in a.materials):
        p.error("invalid material family")
    if any(reg not in VALID_REGIMES for reg in a.regimes):
        p.error("invalid material motion regime")
    if any(budget < 32 or budget > 10000 for budget in a.budgets):
        p.error("material budgets must be 32..10000")
    if any(x not in {"free", "pull", "fold"} for x in a.coupled_scenarios):
        p.error("invalid coupled scenario")
    binaries = {
        key: path.resolve()
        for key, path in (("exporter", a.exporter), ("replayer", a.replayer),
                          ("coupled", a.coupled))
    }
    if not all(path.is_file() for path in binaries.values()):
        p.error("build native exporter, replayer and coupled audit first")

    a.output.mkdir(parents=True, exist_ok=False)
    for child in ("snapshots", "replays", "logs", "coupled"):
        (a.output / child).mkdir()
    natural = []
    # Treat every generated geometry/topology combination as an independent
    # scene identifier; do not invent statistical significance from frames.
    for material in a.materials:
        for regime in a.regimes:
            for budget in a.budgets:
                first_frame_fingerprints = set()
                for variant in HELDOUT[:a.heldout_count]:
                    label = (material + "-" + regime + "-budget" +
                             str(budget) + "-geom" + str(variant["geometry"]) +
                             "-topo" + str(variant["topology"]))
                    snapshot = a.output / "snapshots" / (label + ".kmcb")
                    replay = a.output / "replays" / (label + ".csv")
                    run([str(binaries["exporter"]), str(snapshot), material,
                         str(budget), str(a.frames), str(variant["motion"]),
                         regime, str(variant["geometry"]),
                         str(variant["topology"])],
                        a.output / "logs" / (label + "-export.log"), a.timeout)
                    meta = snapshot_metadata(snapshot)
                    if meta["frames"] != a.frames or meta["seed"] != variant["motion"]:
                        raise ValueError("Invalid generated source metadata")
                    with snapshot.open("rb") as stream:
                        stream.seek(HEADER.size)
                        first = stream.read(meta["n"] * 24)
                        if len(first) != meta["n"] * 24:
                            raise ValueError("Truncated first frame in geometry")
                        fingerprint = hashlib.sha256(first).hexdigest()
                    if fingerprint in first_frame_fingerprints:
                        raise ValueError(
                            "Independent held-out seed did not alter first-frame geometry")
                    first_frame_fingerprints.add(fingerprint)
                    order = random_order_seed("HELDOUT_V1", label)
                    run([str(binaries["replayer"]), str(snapshot), str(replay),
                         "0", "--order-seed", str(order)],
                        a.output / "logs" / (label + "-replay.log"), a.timeout)
                    ranking = score(replay, skip=a.warmup)
                    check_ordered_replay(replay, order)
                    methods = ranking["methods"]
                    if not {"global_guarded_exact", "guard_sap_bounded_trial",
                            "sap_persistent_coherent", "online_cost_aware",
                            "sap_rebuild"}.issubset(methods):
                        raise ValueError("Original algorithm cohort incomplete")
                    # Each method is measured on the exact same NEW scene.
                    ratio = (methods["global_guarded_exact"]["mean_ms"] /
                             methods["guard_sap_bounded_trial"]["mean_ms"])
                    natural.append({
                        "material": material, "regime": regime,
                        "budget": budget, "actual_n": meta["n"],
                        **variant, "snapshot": str(snapshot),
                        "snapshot_sha256": sha256(snapshot),
                        "first_frame_sha256": fingerprint,
                        "replay": str(replay),
                        "replay_sha256": sha256(replay),
                        "order_seed": order,
                        "methods_actually_executed": sorted(methods),
                        "guard_over_frozen_v4": ratio,
                        "full_frame_sap_pair_parity": True
                    })
                    print("PASS HELDOUT", label, "N", meta["n"],
                          "Guard/V4=" + format(ratio, ".4f") + "x",
                          flush=True)

    coupled = []
    # The coupled contact solver NOW receives the exact same independently
    # assigned geometry/topology seed pairs as the new material fixtures.
    # Its actual triangles, constraints, positions and anchors are varied,
    # not merely the broad-phase AABB stream.
    for variation in HELDOUT[:a.heldout_count]:
        for scenario in a.coupled_scenarios:
            label = (scenario+"-geom"+str(variation["geometry"])+
                     "-topo"+str(variation["topology"]))
            directory = a.output / "coupled" / label
            log = a.output / "logs" / ("coupled-" + label + ".log")
            run([str(binaries["coupled"]), str(directory),
                 str(a.coupled_frames), "2", "2", scenario,
                 str(variation["geometry"]), str(variation["topology"])],
                log, a.timeout)
            data = parse_coupled(
                directory / "COUPLED_SOLVER.csv", a.coupled_frames, scenario,
                variation["geometry"], variation["topology"])
            data["mesh_stream_sha256"] = {
                label: sha256(directory / (label + ".jsonl"))
                for label in data["methods"]
            }
            # Archive the independently generated actual mesh geometry and
            # triangle connectivity, not just its flat AABB envelopes.
            import json as _json
            header = _json.loads(
                (directory / "sap_rebuild.jsonl").open(encoding="utf-8").readline())
            data["source_rest_positions_sha256"] = hashlib.sha256(
                _json.dumps(header["rest_positions"],separators=(",",":")).encode()
            ).hexdigest()
            data["source_triangle_topology_sha256"] = hashlib.sha256(
                _json.dumps(header["triangles"],separators=(",",":")).encode()
            ).hexdigest()
            coupled.append(data)
            print("PASS COUPLED", label, "frames", a.coupled_frames,
                  "independent physical solver mesh/contacts agree with SAP",
                  flush=True)
    # Safety gate: a coupled test without ANY actual precise collision
    # checks or contact responses is merely motion/geometry parity. Reject
    # a deceptive physically vacuous successful report.
    for variation in HELDOUT[:a.heldout_count]:
        matching = [x for x in coupled
                    if x["geometry_seed"] == variation["geometry"] and
                       x["topology_seed"] == variation["topology"]]
        if not any(x["precise_tests_by_method"]["sap_rebuild"] > 0 and
                   x["contacts_by_method"]["sap_rebuild"] > 0 for x in matching):
            raise ValueError(
                "No actual precise tests/contact responses for independently "
                "varied coupled solver mesh " + str(variation))
    for scenario in a.coupled_scenarios:
        tested = [x for x in coupled if x["scenario"] == scenario]
        if len({x["source_rest_positions_sha256"] for x in tested}) != len(tested):
            raise ValueError("Coupled independent geometry seeds repeated original mesh")
        if len({x["source_triangle_topology_sha256"] for x in tested}) != len(tested):
            raise ValueError("Coupled independent topology seeds repeated original triangulation")
    summary = {
        "experiment": "Kyoto independent geometry and topology plus coupled solver audit",
        "frozen_v4_unchanged": True,
        "historical_phase18_inputs_untouched": True,
        "no_training_or_auto_optimization": True,
        "independent_geometry_topology_seeds_used": list(HELDOUT[:a.heldout_count]),
        "new_varied_material_scenes": len(natural),
        "material_replays": natural,
        "coupled_solver_runs": coupled,
        "actual_physical_contact_coverage_enforced": True,
        "important_limits": [
            "Independent geometry and triangle connectivity are now varied in BOTH the mass-spring fixture and coupled MultiCloth solver under explicitly different independent seeds",
            "Mass-spring fixture uses damped Verlet plus constrained spherical/floor projection; it is not a calibrated contact solver",
            "New full candidate-pair replay times do not include narrow-phase or solve time",
            "MultiCloth still has a fixed 13x13 grid connectivity family (no remeshing or tearing), but independent diagonal choices, vertex jitter, pin constraints and structural connections now vary",
            "MultiCloth policy runs are executed in separate consecutive sessions; their end-to-end time is descriptive, not randomized ABBA speedup",
            "No CCD, continuous contact detection, externally validated material constitutive model or penetration-depth reference is claimed",
            "Different seeds guarantee geometry and selected connectivity changes through native fixture tests; forcing phase alone is insufficient",
        ],
    }
    manifest = a.output / "HELDOUT_RESULTS.json"
    manifest.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Kyoto held-out geometry/connectivity and coupled solver evidence", "",
        "Independent coordinate and mesh connectivity seeds were used in "
        "ADDITIONAL mass-spring fixtures; all original V4 policies and "
        "Phase 18 datasets were retained.", "",
        "## New complete AABB candidate output (not solver timing)", "",
        "| Material | Regime | Requested | Actual N | Geometry seed | Topology seed | Guard/V4 query mean ratio |",
        "|---|---|---:|---:|---:|---:|---:|"
    ]
    for x in natural:
        lines.append("| " + x["material"] + " | " + x["regime"] +
                     " | " + str(x["budget"]) + " | " + str(x["actual_n"]) +
                     " | " + str(x["geometry"]) + " | " +
                     str(x["topology"]) + " | " +
                     format(x["guard_over_frozen_v4"], ".4f") + "x |")
    lines += ["", "## Existing coupled-contact solver correctness and physical metrics", "",
              "| Scenario | Geometry seed | Topology seed | Frames | Three original policies | SAP geometry equal? | SAP contact events | Mean full step ms (SAP / Guard / V4) |",
              "|---|---:|---:|---:|---|---|---:|---|"]
    for x in coupled:
        times = x["step_time_mean_ms_by_method"]
        lines.append("| " + x["scenario"] + " | " + str(x["geometry_seed"]) +
                     " | " + str(x["topology_seed"]) +
                     " | " + str(x["frames"]) +
                     " | SAP, Guard, frozen V4 | yes | " +
                     str(x["contacts_by_method"]["sap_rebuild"]) + " | " +
                     " / ".join(format(times[k], ".4f") for k in (
                         "sap_rebuild", "global_guarded", "guard_sap_bounded_trial")) +
                     " |")
    lines += ["", "## Methodological boundaries", ""]
    lines += ["- " + text for text in summary["important_limits"]]
    (a.output / "HELDOUT_RESULTS.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")
    print("PASS: new topologically varied source snapshots and independent "
          "exact candidate pair oracles; separate coupled contact solver audit",
          flush=True)
    print(a.output / "HELDOUT_RESULTS.md")


if __name__ == "__main__":
    main()
