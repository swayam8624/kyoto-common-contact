#!/usr/bin/env python3
"""Fail-closed test for the NEW RBC full-pair material replay.

Smoke tests two real KYOTOM01 worlds with different geometry/topology
seeds, checks all method/repeat/frame slots and byte-for-byte SAP pair
hash parity, and confirms original source/no-overwrite semantics.
All native results are generated in an isolated temporary directory.
No historical held-out evidence or old replay CLI is ever touched.
"""
from __future__ import annotations

import csv
import hashlib
import math
import struct
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

METHODS = (
    "guard_045_A", "guard_045_B", "guard_100", "guard_200",
    "rbc_indexed_045", "rbc_indexed_100", "rbc_indexed_200",
    "sap_rebuild", "sap_persistent",
    "guard_sap_bounded_trial_frozen"
)

def execute(*arguments: str) -> str:
    result = subprocess.run(list(arguments), check=True, text=True,
                            capture_output=True, timeout=120)
    return result.stdout

def check_one(exporter: str,replayer: str, root: Path,
              material: str, geometry: int, topology: int) -> None:
    snapshot=root/f"{material}-{geometry}.kmcb"
    output=root/f"{material}-{geometry}.csv"
    execute(exporter,str(snapshot),material,"128","36",
            "197003","rapid",str(geometry),str(topology))
    blob=snapshot.read_bytes()
    sha=hashlib.sha256(blob).hexdigest()
    assert blob[:8]==b"KYOTOM01"
    n,frames,seed,material_code=struct.unpack_from("<4I",blob,8)
    assert n>0 and frames==36 and seed==197003
    assert len(blob)==24+n*frames*24
    message=execute(replayer,str(snapshot),str(output),"2","8")
    assert "PASS RBC full-pair replay" in message
    with output.open(newline="",encoding="utf-8") as fd:
        rows=list(csv.DictReader(fd))
    assert len(rows)==len(METHODS)*2*(frames-8)
    groups=defaultdict(list)
    by_oracle=defaultdict(set)
    for row in rows:
        method=row["method"]
        repeat=int(row["repeat"]); frame=int(row["frame"])
        key=(method,repeat)
        assert method in METHODS
        assert repeat in (0,1)
        assert int(row["slot"])==(METHODS.index(method) if repeat==0
                                   else len(METHODS)-1-METHODS.index(method))
        assert 8<=frame<frames
        assert int(row["n"])==n and int(row["source_seed"])==seed
        assert int(row["material_code"])==material_code
        assert row["oracle_equal"]=="1"
        assert int(row["pairs"])>=0
        assert int(row["pair_hash"])>=0
        assert math.isfinite(float(row["ms"])) and float(row["ms"])>=0
        for field in ("cumulative_builds","cumulative_reuses",
                      "potential_pairs","cumulative_current_checks",
                      "cumulative_x_sweep_tests"):
            assert int(row[field])>=0
        for field in ("v4_sap","v4_trial","v4_switched"):
            assert int(row[field]) in (0,1)
        groups[key].append(frame)
        by_oracle[(repeat,frame)].add((row["pairs"],row["pair_hash"]))
    assert set(groups)=={(m,r) for m in METHODS for r in (0,1)}
    assert all(v==list(range(8,frames)) for v in groups.values())
    assert all(len(v)==1 for v in by_oracle.values())
    assert len(by_oracle)==2*(frames-8)
    try:
        execute(replayer,str(snapshot),str(output),"2","8")
    except subprocess.CalledProcessError:
        pass
    else:
        raise AssertionError("RBC native replay wrongly overwrote original evidence")
    assert hashlib.sha256(snapshot.read_bytes()).hexdigest()==sha
    print(f"PASS RBC source={material} native_N={n} frames={frames} "
          f"original_sha256={sha} distinct_methods={len(METHODS)} "
          f"verified_rows={len(rows)}")

def main() -> None:
    if len(sys.argv)!=3:
        raise SystemExit("usage: test_rbc_material_replay.py EXPORTER REPLAYER")
    with tempfile.TemporaryDirectory(prefix="rbc-replay-smoke-") as td:
        folder=Path(td)
        check_one(sys.argv[1],sys.argv[2],folder,
                  "cloth",73013,86011)
        check_one(sys.argv[1],sys.argv[2],folder,
                  "sponge",74017,86019)
    print("PASS: TWO independently generated meshed native trajectories,"
          " no original overwrite, 10-method exact hashes on all frames")

if __name__=="__main__":
    main()
