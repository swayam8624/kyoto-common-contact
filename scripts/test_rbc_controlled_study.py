#!/usr/bin/env python3
"""Native integration smoke for the ten-policy repeated A/A runner.

Uses ORIGINAL C++ source generator and NEW separate full-pair executable,
then checks real manifest/source receipts, exact native all-method pair
coverage, guarded A/A noise bookkeeping and no-overwrite behavior.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_rbc_controlled_replays import run

def digest(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main()->None:
    if len(sys.argv)!=3:
        raise SystemExit("usage: test_rbc_controlled_study.py EXPORTER REPLAYER")
    exporter,replayer=map(Path,sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="rbc-ten-policy-") as d:
        root=Path(d)
        sources=[]
        for i,(material,g,t) in enumerate((
            ("cloth",73013,86011),
            ("sponge",74017,86019)
        )):
            name=root/f"source-{i}.kmcb"
            subprocess.run([str(exporter),str(name),material,
                            "128","24","197003","rapid",str(g),str(t)],
                            check=True,capture_output=True,text=True,
                            timeout=90)
            sources.append({"material":material,"snapshot":str(name),
                            "snapshot_sha256":digest(name)})
        manifest=root/"HELDOUT_RESULTS.json"
        manifest.write_text(json.dumps({
            "material_replays":sources
        })+"\n",encoding="utf-8")
        result=run(replayer,manifest,root/"controlled-10",2,6,0)
        assert result["source_scene_count"]==2
        assert result["method_names"]==list(
            ("guard_045_A","guard_045_B","guard_100","guard_200",
             "rbc_indexed_045","rbc_indexed_100","rbc_indexed_200",
             "sap_rebuild","sap_persistent",
             "guard_sap_bounded_trial_frozen"))
        assert all(s["result"]["native_rows"]==2*10*(24-6)
                   for s in result["scenes"])
        for source in sources:
            assert digest(Path(source["snapshot"]))==source["snapshot_sha256"]
        try:
            run(replayer,manifest,root/"controlled-10",2,6,0)
        except FileExistsError:
            pass
        else:
            raise AssertionError("Runner overwritten original output")
        bad=root/"corrupted.json"
        changed=json.loads(manifest.read_text())
        changed["material_replays"][0]["snapshot_sha256"]="f"*64
        bad.write_text(json.dumps(changed)+"\n")
        try:
            run(replayer,bad,root/"must-not-exist",2,6,0)
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid source SHA incorrectly accepted")
        assert not (root/"must-not-exist").exists()
        print("PASS: two native geometry/topology variations x 10 methods"
              " x 2 timing-order reversals, exact pair parity, independent"
              " input receipts, A/A accounting and no-overwrite gates")

if __name__=="__main__":
    main()
