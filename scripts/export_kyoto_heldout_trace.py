#!/usr/bin/env python3
"""Export compact source-verified V4/Guard decision traces without native reruns.

Useful for an actual Mac/Linux parity comparison of policy decisions and
bitwise-source identities, rather than merely equal aggregate action counts.

Reads an existing intact run_kyoto_heldout_research.py output directory.
Refuses to overwrite existing output. The original immutable snapshots,
per-method CSVs, timings, selected-policy actions, source SHA-256 receipts
and native replay correctness checks are NEVER changed.

Example:
  python3 scripts/export_kyoto_heldout_trace.py \
    --results output/kyoto-mac-matched-60f \
    --output output/kyoto-mac-matched-60f-source-trace.json.gz
"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import (
    check_ordered_replay, sha256, snapshot_metadata
)

CHOICES = {"global_guarded_exact", "sap_persistent_coherent"}


def positive_time(value: str) -> float:
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise ValueError("Original native timing is nonfinite or nonpositive")
    return result


def same_frame(rows: dict[str, dict[int, dict[str, str]]], method: str,
               frame: int) -> dict[str, str]:
    return rows[method][frame]


def verify_scene(item: dict) -> dict:
    """Read every ORIGINAL row and check exact per-frame SAP pair hashes."""
    snapshot = Path(item["snapshot"])
    replay = Path(item["replay"])
    if not snapshot.is_file() or sha256(snapshot) != item["snapshot_sha256"]:
        raise ValueError("Original KYOTOM01 snapshot is missing or tampered: " + str(snapshot))
    if not replay.is_file() or sha256(replay) != item["replay_sha256"]:
        raise ValueError("Original native full-policy CSV is missing or tampered: " + str(replay))

    metadata = snapshot_metadata(snapshot)
    frames = metadata["frames"]
    if metadata["n"] != item["actual_n"] or metadata["seed"] != item["motion"]:
        raise ValueError("Original source geometry header differs from the receipt")
    ordered_slots = check_ordered_replay(replay, int(item["order_seed"]))
    data: dict[str, dict[int, dict[str, str]]] = defaultdict(dict)
    with replay.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        required = {"backend", "frame", "ms", "pairs", "pair_hash", "misses",
                    "execution_slot", "order_seed", "chosen_backend",
                    "hybrid_probed", "hybrid_switched"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("Native original CSV missing required provenance/action columns")
        for row in reader:
            name = row["backend"]
            frame = int(row["frame"])
            if name not in ordered_slots or frame < 0 or frame >= frames:
                raise ValueError("Unexpected method/frame identity in original native file")
            if frame in data[name]:
                raise ValueError("Duplicate original method/frame observation")
            if int(row["misses"]) != 0 or not row["pair_hash"]:
                raise ValueError("Native method reported incorrect or missing exact AABB candidates")
            positive_time(row["ms"])
            data[name][frame] = row
    if set(data) != set(item["methods_actually_executed"]):
        raise ValueError("Native original method cohort changed after manifest creation")
    if not {"sap_rebuild", "global_guarded_exact",
            "guard_sap_bounded_trial"}.issubset(data):
        raise ValueError("Required SAP/Guard/V4 exact source observations missing")
    if any(set(observed) != set(range(frames)) for observed in data.values()):
        raise ValueError("Non-contiguous original per-frame policy observations")

    trace = []
    for frame in range(frames):
        sap = same_frame(data, "sap_rebuild", frame)
        guard = same_frame(data, "global_guarded_exact", frame)
        v4 = same_frame(data, "guard_sap_bounded_trial", frame)
        oracle = (sap["pairs"], sap["pair_hash"])
        if any((data[name][frame]["pairs"], data[name][frame]["pair_hash"]) != oracle
               for name in data):
            raise ValueError("Source frame has mismatched exact SAP candidate count/hash")
        if v4["chosen_backend"] not in CHOICES:
            raise ValueError("Native V4 selected unsupported backend")
        trial = int(v4["hybrid_probed"])
        switched = int(v4["hybrid_switched"])
        if trial not in (0, 1) or switched not in (0, 1):
            raise ValueError("Native V4 trial/switch fields are not boolean")
        if trial and v4["chosen_backend"] != "sap_persistent_coherent":
            raise ValueError("V4 claimed an SAP trial while choosing Guard")
        trace.append({
            "f": frame,
            "pairs": int(sap["pairs"]),
            "pair_hash": sap["pair_hash"],
            "guard_ms": positive_time(guard["ms"]),
            "v4_ms": positive_time(v4["ms"]),
            "v4_backend": v4["chosen_backend"],
            "v4_trial": trial,
            "v4_switch": switched,
        })
    return {
        "material": item["material"],
        "regime": item["regime"],
        "actual_n": item["actual_n"],
        "requested_budget": item["budget"],
        "motion_seed": item["motion"],
        "geometry_seed": item["geometry"],
        "topology_seed": item["topology"],
        "source_snapshot_name": snapshot.name,
        "source_snapshot_sha256": item["snapshot_sha256"],
        "original_replay_name": replay.name,
        "original_replay_sha256": item["replay_sha256"],
        "ordered_slots": ordered_slots,
        "frames": frames,
        "observations": trace,
    }


def export(root: Path) -> dict:
    manifest = root / "HELDOUT_RESULTS.json"
    if not manifest.is_file():
        raise ValueError("Missing source heldout manifest")
    original = json.loads(manifest.read_text(encoding="utf-8"))
    if original.get("experiment") != (
        "Kyoto independent geometry and topology plus coupled solver audit"
    ):
        raise ValueError("Not a native original heldout source manifest")
    if not original.get("frozen_v4_unchanged") or not original.get(
        "historical_phase18_inputs_untouched"
    ):
        raise ValueError("Original frozen method control was not preserved")
    scenes = [verify_scene(x) for x in original["material_replays"]]
    expected_count = original["new_varied_material_scenes"]
    if len(scenes) != expected_count or not scenes:
        raise ValueError("Original native scene count incorrect or empty")
    scenes.sort(key=lambda x: (
        x["material"], x["regime"], x["actual_n"],
        x["motion_seed"], x["geometry_seed"], x["topology_seed"]
    ))
    keys = [(
        x["material"], x["regime"], x["actual_n"],
        x["motion_seed"], x["geometry_seed"], x["topology_seed"]
    ) for x in scenes]
    if len(keys) != len(set(keys)):
        raise ValueError("Original heldout scene identity duplicated")
    return {
        "schema": "kyoto-heldout-source-verified-trace-v1",
        "experiment": "Original immutable full-method replay; reduced exact action/timing evidence",
        "original_manifest_sha256": sha256(manifest),
        "original_scene_count": len(scenes),
        "original_method_counts": sorted({
            len(s["ordered_slots"]) for s in scenes
        }),
        "native_benchmark_rerun": False,
        "original_results_mutated": False,
        "source_bitwise_identity_with_other_platform_not_assumed": True,
        "trace_notes": [
            "Each original snapshot and full-method CSV SHA-256 was verified before extraction",
            "Every emitted frame was compared against original SAP full pair hash/count across all executed methods",
            "Original native per-method execution slots, chosen V4 backend, trial/switch and current frame query times retained",
            "Identical controller action counts do not establish identical per-frame action sequences or identical source bytes",
            "Native process/hardware thermal and policy-order confounds remain; this trace is not interleaved ABBA timing",
            "This is not a physics penetration, CCD, full solver, external material or self-contact oracle"
        ],
        "scenes": scenes
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite existing compact trace: " + str(args.output))
    if not args.output.name.endswith(".json.gz"):
        parser.error("Output must end with .json.gz")
    report = export(args.results)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        with args.output.open("xb") as stream:
            created = True
            with gzip.GzipFile(fileobj=stream, mode="wb", mtime=0,
                               filename="") as compressed:
                compressed.write((json.dumps(
                    report, sort_keys=True, separators=(",", ":")
                ) + "\n").encode("utf-8"))
    except BaseException:
        # Atomic create-exclusive protects concurrent evidence producers.
        # Do NOT unlink an existing output that someone else created.
        if created and args.output.exists():
            args.output.unlink()
        raise
    print("PASS: original binary/CSV receipts and per-frame SAP candidate "
          "fingerprints reverified; compact V4 source trace generated")
    print("scenes:", report["original_scene_count"],
          "per-scene native method counts:", report["original_method_counts"],
          "output:", args.output,
          "sha256:", sha256(args.output))


if __name__ == "__main__":
    main()
