#!/usr/bin/env python3
"""Small fail-closed fixtures for evidence CSV summarizers."""
import csv
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"


def write_csv(path, header, rows):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(header)
        writer.writerows(rows)


class EvidenceSummarizerTests(unittest.TestCase):
    def run_script(self, script, *args, success=True):
        result = subprocess.run([sys.executable, str(SCRIPTS / script), *map(str, args)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success,
                         msg=f"{script}: {result.stdout}\n{result.stderr}")
        return result

    def test_paired_benchmark_requires_complete_unique_samples(self):
        header = ("trial", "frame", "policy", "stepMs", "broadMs", "narrowMs",
                  "oracleMs", "diagnosticMs", "otherMs", "preciseTests", "oracleMisses")
        rows = []
        for trial in range(5):
            for frame in range(30, 300):
                for policy in ("baseline_rebuild", "guarded_reuse"):
                    rows.append((trial, frame, policy, 1, .2, .1, 0, .01, .69, 4, "NA"))
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            source = temp / "live.csv"
            write_csv(source, header, rows)
            self.run_script("summarize_benchmark.py", source, temp / "chart.svg", temp / "report.md")
            self.run_script("summarize_pipeline.py", source, temp / "cascade.svg")

            duplicate = rows + [rows[0]]
            write_csv(source, header, duplicate)
            self.run_script("summarize_benchmark.py", source, temp / "dup.svg", temp / "dup.md", success=False)

            write_csv(source, header, rows[2:])
            self.run_script("summarize_pipeline.py", source, temp / "missing.svg", success=False)

    def test_online_trace_checks_integer_flags_and_contiguous_frames(self):
        header = ("frame", "onlineStepMs", "sapStepMs", "onlineBroadMs", "sapBroadMs",
                  "onlineNarrowMs", "onlineDecisionMs", "onlinePatches", "onlineSplits",
                  "onlineMerges", "onlineStrategySwitches", "onlineHierarchy",
                  "contactCorrections", "interSheetCorrections", "onlineCandidates",
                  "sapCandidates", "regionDirty", "regionBuilds", "regionReuses",
                  "coarseSkip", "observedAabbWork", "observedContactHeat",
                  "observedCoarseOverlapFraction", "maxPositionError", "coverageChecked")
        rows = []
        for frame in range(1, 121):
            rows.append([frame, 1, 1.1, .2, .3, .1, .001, 12, int(frame == 12), 0,
                         0, 1, 0, 0, 5, 5, 0, 1, 2, 1, 5, 0, .2, 0,
                         int(frame == 1 or frame % 15 == 0)])
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            source = temp / "trace.csv"
            write_csv(source, header, rows)
            self.run_script("summarize_online_trace.py", source, temp / "trace.svg", temp / "report.md")
            rows[20][header.index("onlineSplits")] = "0.5"
            write_csv(source, header, rows)
            self.run_script("summarize_online_trace.py", source, temp / "bad.svg", temp / "bad.md", success=False)

    def test_leaderboard_requires_full_same_size_32_sample_matrix(self):
        header = ("scene", "algorithm", "primitives", "samples", "meanMs", "medianMs",
                  "p95Ms", "meanCandidates", "meanExtraCandidates", "builds", "reuses",
                  "misses", "updates")
        scenes = ("sparse256", "clustered512", "dense256", "sheets2", "sheets3", "simulated_cloths3")
        algorithms = ("exhaustive_aabb", "sweep_prune", "bullet_dbvt", "bullet_dbvt_dynamic",
                      "guarded_sap", "region_fixed", "region_adaptive", "region_sweep_fixed",
                      "region_sweep_adaptive")
        rows = [[scene, algo, 256, 32, .1, .1, .2, 4, 0, 32, 0, 0, 0]
                for scene in scenes for algo in algorithms]
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            source = temp / "leaderboard.csv"
            write_csv(source, header, rows)
            self.run_script("summarize_leaderboard.py", source, temp / "chart.svg", temp / "report.md")
            rows[0][3] = 31
            write_csv(source, header, rows)
            self.run_script("summarize_leaderboard.py", source, temp / "bad.svg", temp / "bad.md", success=False)

    def test_multicloth_requires_one_consistent_positive_frame_count(self):
        policies = ("sap_rebuild", "global_guarded", "region_fixed", "region_adaptive",
                    "region_sweep_fixed", "region_sweep_adaptive", "online_contact_work")
        header = ("sheets", "policy", "frames", "meanBroadMs", "meanNarrowMs", "meanStepMs",
                  "meanCandidates", "meanInterContacts", "meanSphereContacts", "meanRegionBuilds",
                  "meanRegionReuses", "meanRejected", "meanPreciseTests", "maxPositionError",
                  "meanTopLevelTests", "meanActiveRegionPairs", "meanSkippedRegionPairs",
                  "meanOnlinePatches", "meanSplits", "meanMerges", "meanStrategySwitches",
                  "meanOnlineDecisionMs")
        rows = []
        for sheets in (1, 2, 3):
            for policy in policies:
                values = [sheets, policy, 40, .1, .1, .2, 1,
                          int(sheets > 1), 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]
                rows.append(values)
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            source = temp / "multicloth.csv"
            write_csv(source, header, rows)
            self.run_script("summarize_multicloth.py", source, temp / "chart.svg", temp / "report.md")
            rows[-1][2] = 39
            write_csv(source, header, rows)
            self.run_script("summarize_multicloth.py", source, temp / "bad.svg", temp / "bad.md", success=False)

    def test_patch_sweep_requires_matching_phase_frame_counts(self):
        header = ("patchSpan", "frontier", "phase", "frames", "meanStepMs", "meanBroadMs",
                  "meanNarrowMs", "meanCandidates", "meanSkippedPatchPairs", "meanRegionBuilds",
                  "meanRegionReuses", "meanInterContacts", "maxPositionError")
        rows = []
        configurations = [(0, "sap_rebuild")] + [
            (span, frontier) for frontier in ("all_region_pairs", "two_tier")
            for span in (1, 2, 3, 4, 6, 12)]
        for span, frontier in configurations:
            for phase in ("calibration", "evaluation"):
                rows.append([span, frontier, phase, 20, .2, .1, .05, 2, 1, 1, 1, 1, 0])
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            source = temp / "patch.csv"
            write_csv(source, header, rows)
            self.run_script("summarize_patch_sweep.py", source, temp / "chart.svg", temp / "report.md")
            rows[-1][3] = 19
            write_csv(source, header, rows)
            self.run_script("summarize_patch_sweep.py", source, temp / "bad.svg", temp / "bad.md", success=False)

    def test_prepare_script_uses_pipefail_shell(self):
        script = (SCRIPTS / "prepare_meeting.sh").read_text(encoding="utf-8")
        self.assertTrue(script.startswith("#!/bin/bash\n"))
        self.assertIn("set -euo pipefail", script)
        result = subprocess.run(["/bin/bash", "-c", "set -euo pipefail; false | tee /dev/null"],
                                capture_output=True)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
