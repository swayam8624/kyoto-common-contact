#!/usr/bin/env python3
"""Pass 18: fail-closed native source audit of truly accepted FEM CCD leaves.

Inputs are emitted by the *native C++ physics + official IPC* executable,
not by a Python synthetic scene. Tests event coverage, complete physical
elapsed time, accepted finite-triangle geometry and independent enumeration.
Zero actual CCD events is reported as zero; never invented to claim success.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
from pathlib import Path

COLS = (
    "leaf_id", "start_time_s", "end_time_s", "accepted_dt_s",
    "full_face_gap_m", "cross_vf_stencils", "cross_ee_stencils",
    "official_CCD_vf_events", "official_CCD_ee_events",
    "events_missing_from_independent_AABB_oracle",
    "events_missing_from_official_LBVH",
    "events_missing_from_maintained_hierarchy", "earliest_upstream_toi",
)

def report(path: Path) -> dict:
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = set(COLS) - set(reader.fieldnames or ())
        if missing:
            raise ValueError(f"invalid original CCD source columns: {sorted(missing)}")
        rows = list(reader)
    if len(rows) < 2:
        raise ValueError("fast-impact case must have multiple ACCEPTED physical leaves")
    last_end = 0.0
    totals = {k: 0 for k in COLS if k.startswith("cross_") or
              k.startswith("official_CCD") or k.startswith("events_missing")}
    minimum_gap = math.inf
    event_leaves = 0
    for index, r in enumerate(rows):
        sample = {}
        for k in COLS:
            try:
                sample[k] = float(r[k])
            except (TypeError, ValueError, KeyError) as exc:
                raise ValueError(f"invalid original CCD value {k} leaf {index}") from exc
            if not math.isfinite(sample[k]):
                raise ValueError(f"nonfinite {k} leaf {index}")
        if sample["leaf_id"] != index:
            raise ValueError("non-contiguous original source leaf IDs")
        start, end, dt = (sample[k] for k in
                          ("start_time_s", "end_time_s", "accepted_dt_s"))
        if dt <= 0 or end <= start or abs(start-last_end)>1e-11 or abs(end-start-dt)>1e-11:
            raise ValueError(f"non-contiguous or incomplete accepted elapsed time at leaf {index}")
        if sample["full_face_gap_m"] <= 0:
            raise ValueError(f"rigid finite triangle penetration in accepted leaf {index}")
        minimum_gap = min(minimum_gap, sample["full_face_gap_m"])
        toi = sample["earliest_upstream_toi"]
        if not (0 <= toi <= 1):
            raise ValueError(f"invalid actual upstream TOI at leaf {index}")
        for k in totals:
            n = sample[k]
            if n < 0 or not n.is_integer():
                raise ValueError(f"invalid event/stencil counter {k} at leaf {index}")
            totals[k] += int(n)
        if sample["official_CCD_vf_events"] + sample["official_CCD_ee_events"] > 0:
            event_leaves += 1
        last_end = end
    if abs(last_end - 0.00075)>1e-11:
        raise ValueError("fast real FEM accepted leaves do not sum to 0.75 ms")
    for k in totals:
        if k.startswith("events_missing") and totals[k]:
            raise ValueError(f"actual upstream CCD events were culled by {k}")
    return {
        "source": "native 20m/s accepted neo-Hookean FEM leaves + pinned official IPC v1.6.1",
        "accepted_physical_leaves": len(rows),
        "elapsed_time_s": last_end,
        "minimum_accepted_face_gap_m": minimum_gap,
        "actual_ipc_event_bearing_leaves": event_leaves,
        **totals,
        "interpretation": (
            "All cross-object upstream IPC CCD events found by exhaustive "
            "feature enumeration were retained by both broadphases. "
            "This does not prove independent CCD numerical correctness, "
            "arbitrary-speed stability, deformable self-contact response, "
            "or any full-physics solver speedup."
        ),
    }

def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    result=report(a.input)
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"pass18_actual_ccd_source_audit.json").write_text(
        json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2,sort_keys=True))

if __name__=="__main__":
    main()
