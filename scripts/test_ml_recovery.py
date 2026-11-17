#!/usr/bin/env python3
"""Validate interruption packaging, original evidence preservation and append-only logs.

Runs without PyTorch; native learned policy correctness is covered by CTest.
"""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
RUNNER=ROOT/"scripts/run_kyoto_ml_mac.py"
PACKAGER=ROOT/"scripts/package_kyoto_results.py"

class RecoveryTests(unittest.TestCase):
    def test_interruption_archives_only_diagnostics_and_keeps_full_inputs(self):
        with tempfile.TemporaryDirectory(prefix="kyoto-recovery-") as directory:
            root=Path(directory)/"experiment"
            for name in ("logs","model","teacher","snapshots","heldout"):
                (root/name).mkdir(parents=True)
            (root/"logs/torch-training.log").write_text("[TRAIN] trial=34/156\n")
            (root/"model/trials.jsonl").write_text('{"trial": 0}\n')
            (root/"teacher/data-cpu.csv").write_bytes(b"original teacher")
            (root/"snapshots/data.kmcb").write_bytes(b"original geometry")
            (root/"heldout/evidence.csv").write_bytes(b"original holdout")
            subprocess.run([sys.executable,str(PACKAGER),str(root),
                            "--exit-code","1","--diagnostics-only"],
                           check=True,capture_output=True,text=True)
            partials=list(root.parent.glob("experiment-partial-*.zip"))
            self.assertEqual(len(partials),1)
            with zipfile.ZipFile(partials[0]) as archive:
                names=archive.namelist()
                self.assertIn("experiment/DIAGNOSTIC_MANIFEST.json",names)
                self.assertIn("experiment/logs/torch-training.log",names)
                self.assertIn("experiment/model/trials.jsonl",names)
                self.assertFalse(any("/teacher/" in n or "/snapshots/" in n or
                                     "/heldout/" in n for n in names))
                manifest=json.loads(archive.read(
                    "experiment/DIAGNOSTIC_MANIFEST.json"))
                self.assertEqual(manifest["status"],"failed_or_partial")
                self.assertEqual(manifest["artifact_scope"],"diagnostics_only")
            self.assertEqual((root/"teacher/data-cpu.csv").read_bytes(),b"original teacher")
            self.assertEqual((root/"snapshots/data.kmcb").read_bytes(),b"original geometry")
            subprocess.run([sys.executable,str(PACKAGER),str(root),
                            "--exit-code","0"],check=True,capture_output=True,text=True)
            with zipfile.ZipFile(root.with_suffix(".zip")) as archive:
                names=archive.namelist()
                self.assertIn("experiment/teacher/data-cpu.csv",names)
                self.assertIn("experiment/snapshots/data.kmcb",names)
                self.assertIn("experiment/RUN_STATUS.json",names)
            # Complete original results can never be overwritten by a rerun.
            duplicate=subprocess.run([sys.executable,str(PACKAGER),str(root),
                                      "--exit-code","0"],capture_output=True)
            self.assertNotEqual(duplicate.returncode,0)

    def test_subprocess_logs_append_without_erasing_prior_trial_receipts(self):
        spec=importlib.util.spec_from_file_location("kyoto_ml_runner_test",RUNNER)
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix="kyoto-log-") as directory:
            log=Path(directory)/"training.log"
            log.write_text("[TRAIN] trial=34/156 ORIGINAL\n")
            module.call([sys.executable,"-c","print('[TRAIN] trial=35/156 CONTINUED')"],
                        log,ROOT,30)
            content=log.read_text()
            self.assertIn("trial=34/156 ORIGINAL",content)
            self.assertIn("trial=35/156 CONTINUED",content)
            self.assertIn("[RESUME]",content)

if __name__=="__main__":
    unittest.main()
