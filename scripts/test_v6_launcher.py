#!/usr/bin/env python3
"""Regression: V6 macOS Bash strict mode handles disjoint seed splits and resume.

Mocks external executables but executes the *real* shell launcher with set -u.
Tests argument assembly without compiling, generating snapshots, or touching
experimental data. CI runs on Ubuntu and native macOS.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class V6LauncherTests(unittest.TestCase):
    def test_fresh_and_resume_on_strict_bash(self):
        source=Path(__file__).resolve().parent/"run_kyoto_unified_v6_mac.sh"
        with tempfile.TemporaryDirectory(prefix="kyoto-rumble-launcher-") as tmp:
            root=Path(tmp)
            scripts=root/"scripts"
            scripts.mkdir()
            shutil.copy2(source,scripts/source.name)
            binaries=root/"mock-bin"
            binaries.mkdir()
            log=root/"captured-python-argv.txt"

            # The runner builds and tests first. Replace those commands
            # with no-op mocks. c++ must create the executable it invokes.
            for name in ("cmake","ctest"):
                exe=binaries/name
                exe.write_text("#!/bin/sh\nexit 0\n",encoding="utf-8")
                exe.chmod(0o755)
            cc=binaries/"c++"
            cc.write_text(
                "#!/bin/sh\n"
                "while [ \"$#\" -gt 0 ]; do\n"
                "  if [ \"$1\" = '-o' ]; then shift; "
                "printf '#!/bin/sh\\nexit 0\\n' > \"$1\"; "
                "chmod +x \"$1\"; break; fi\n"
                "  shift\n"
                "done\n",
                encoding="utf-8")
            cc.chmod(0o755)
            python=binaries/"python3"
            python.write_text(
                "#!/bin/sh\n"
                "printf '%s\\n' \"$*\" >> \"$MOCK_PYTHON_LOG\"\n"
                "exit 0\n",
                encoding="utf-8")
            python.chmod(0o755)
            env=os.environ.copy()
            env["PATH"]=str(binaries)+os.pathsep+env.get("PATH","")
            env["MOCK_PYTHON_LOG"]=str(log)
            env["KYOTO_V6_BUILD"]=str(root/"build")
            env["KYOTO_V6_MAX_EXPERIMENTS"]="12"
            env["KYOTO_V6_REPEATS"]="1"
            (root/"build").mkdir()

            def run(resume):
                log.write_text("",encoding="utf-8")
                env["KYOTO_V6_RESUME"]="1" if resume else "0"
                result=subprocess.run(
                    ["bash",str(scripts/source.name),
                     str(root/"controller.mlpc"),str(root/"fresh-output")],
                    cwd=root,env=env,capture_output=True,text=True,timeout=15,
                    check=False)
                if result.returncode:
                    raise AssertionError(
                        f"Shell launcher failed (resume={resume}):\n"
                        f"{result.stdout}\n{result.stderr}")
                entries=log.read_text(encoding="utf-8").splitlines()
                command=next((v for v in entries
                              if v.startswith("scripts/run_kyoto_controller_ablation.py ")),None)
                self.assertIsNotNone(command,entries)
                self.assertIn("--max-experiments 12",command)
                self.assertIn("--repeats 1",command)
                self.assertEqual("--resume" in command,resume)

            run(False)
            run(True)
            # Independent confirmation uses disjoint seed IDs. The launcher
            # must honor them instead of silently reusing the pilot cohort.
            env["KYOTO_V6_SEEDS"]="5101 5113 5147 5153 5171 5197 5231 5261"
            run(False)
            command=next(v for v in log.read_text(encoding="utf-8").splitlines()
                         if v.startswith("scripts/run_kyoto_controller_ablation.py "))
            self.assertIn("--seeds 5101 5113 5147",command)
            self.assertNotIn("--seeds 4103 4111 4127",command)


if __name__=="__main__":
    unittest.main()
