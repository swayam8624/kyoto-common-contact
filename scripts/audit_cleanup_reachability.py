#!/usr/bin/env python3
"""NON-DESTRUCTIVE, entire tracked-tree review and cleanup classification.

This script deliberately NEVER marks a file safe to delete. It checks the
complete tracked tree, CMake, workflow, import, launcher, documentation and
reference occurrences; outputs a review manifest and immutable inventory.
A missing textual reference is a question for a human, not proof of reachability.
No Git history is rewritten and no original evidence is altered.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

SOURCE_EXT = {".cpp", ".cc", ".cxx", ".m", ".mm", ".hpp", ".h", ".c", ".cu", ".metal"}
TEXT_EXT = SOURCE_EXT | {".py", ".md", ".txt", ".json", ".yml", ".yaml",
                         ".cmake", ".sh", ".html", ".js", ".mjs", ".css", ".csv"}
KEEP_ALWAYS = {"CMakeLists.txt", "CMakePresets.json", "DEVLOG.md",
               "README.md", "FINAL_RELEASE.md", ".gitignore",
               "include/ccp/final_broadphase.hpp",
               "include/ccp/live_cloth.hpp", "src/live_cloth.cpp",
               "include/ccp/research_multicloth.hpp",
               "src/research_multicloth.cpp",
               "include/ccp/online_partitioner.hpp",
               "src/online_partitioner.cpp",
               "include/ccp/material_scenes.hpp",
               "apps/material_replay_benchmark.cpp",
               "apps/material_snapshot_export.cpp",
               "scripts/run_kyoto_heldout_research.py",
               "scripts/analyze_kyoto_heldout_evidence.py",
               "scripts/export_kyoto_heldout_trace.py",
               "scripts/compare_kyoto_heldout_traces.py",
               "docs/CBP_RESEARCH_CHARTER.md",
               "docs/KYOTO_MAC_LINUX_MATCHED_60F_2026-11-19.md"}
CITATION_EXT = {".md", ".txt", ".json"}


def run(root: Path, args: list[str]) -> bytes:
    return subprocess.run(args, cwd=root, check=True,
                          capture_output=True).stdout


def tracked_files(root: Path) -> list[str]:
    data = run(root, ["git", "ls-files", "-z"])
    return sorted(x.decode("utf-8") for x in data.split(b"\0") if x)


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as src:
        for block in iter(lambda: src.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def output_category(path: str, bytes_: int) -> str:
    if path.startswith("output/"):
        return "TRACKED_EVIDENCE_OR_RENDER_ARCHIVE_ONLY"
    if path.startswith("docs/CBP_PHASE") or path.startswith("docs/CBP_FIRST"):
        return "HISTORICAL_DOC_KEEP_CURRENTLY_LINKED"
    if path.startswith(("include/", "src/", "apps/", "tests/", "native/", "viewer/")):
        return "SOURCE_REACHABILITY_REVIEW"
    if path.startswith(".github/"):
        return "CI_WORKFLOW_KEEP_UNTIL_REFERENCES_VERIFIED"
    return "GENERAL_REVIEW"


def audit(root: Path) -> dict:
    names = tracked_files(root)
    files = [(name, root / name) for name in names]
    # Every file gets a SHA-256 receipt so external Release/Zenodo archival
    # can be cross-checked *before* any future tracked binary is removed.
    rows = []
    for name, path in files:
        if not path.is_file():
            raise FileNotFoundError("Tracked file absent: " + name)
        rows.append({
            "path": name, "bytes": path.stat().st_size,
            "sha256": digest(path),
            "category": output_category(name, path.stat().st_size),
        })
    indices = {row["path"]: row for row in rows}
    content = {}
    for name, path in files:
        if (path.suffix.lower() in TEXT_EXT or
                name in {"CMakeLists.txt", ".gitignore", "CMakePresets.json"}):
            if path.stat().st_size <= 2_000_000:
                try:
                    content[name] = path.read_text("utf-8")
                except UnicodeDecodeError:
                    pass
    cmake = content.get("CMakeLists.txt", "")
    workflows = {k: v for k, v in content.items() if k.startswith(".github/workflows/")}
    runtime = {k: v for k, v in content.items() if
               k.endswith((".py", ".sh", ".js", ".mjs", ".cpp", ".hpp", ".mm", ".html"))
               and k not in workflows}
    docs = {k: v for k, v in content.items() if
            k.endswith((".md", ".txt")) and k not in workflows}
    # One portable, syntactic dependency sweep, not an attempted dynamic-link
    # or execution graph. CMake path hits and workflow references are strong,
    # documentation mentions are valuable provenance but NOT code reachability.
    for row in rows:
        path = row["path"]
        base = Path(path).name
        suffix = Path(path).suffix
        refs = {
            "cmake_path": path in cmake,
            "workflow_path": sorted(k for k,v in workflows.items() if path in v),
            "runtime_path": sorted(k for k,v in runtime.items()
                                  if k!=path and path in v),
            "documentation_path": sorted(k for k,v in docs.items()
                                        if k!=path and path in v),
            "base_name_mentions": sorted(k for k,v in content.items()
                                         if k!=path and base in v),
            "include_import_mentions": [],
        }
        if suffix in {".h", ".hpp"}:
            pat = re.compile(r'#\s*include\s*["<][^">]*'+
                             re.escape(base)+r'[">]')
            refs["include_import_mentions"] = sorted(
                k for k,v in runtime.items() if pat.search(v))
        if suffix in {".py", ".js", ".mjs"}:
            module = Path(path).stem
            pat = re.compile(r'\b(?:from|import)\s+'+re.escape(module)+r'\b')
            refs["include_import_mentions"] = sorted(
                k for k,v in runtime.items() if k!=path and pat.search(v))
        row["references"] = refs
        row["protected"] = path in KEEP_ALWAYS or path.startswith(
            ("tests/", ".github/workflows/"))
        if row["protected"]:
            row["review_state"] = "KEEP_PROTECTED"
        elif path.startswith("output/"):
            row["review_state"] = "ARCHIVE_EXTERNALLY_WITH_RECEIPT_BEFORE_PRUNE"
        elif refs["cmake_path"] or refs["workflow_path"] or refs["runtime_path"] or refs["include_import_mentions"]:
            row["review_state"] = "KEEP_REFERENCED_RUNTIME_BUILD_OR_WORKFLOW"
        elif refs["documentation_path"] or refs["base_name_mentions"]:
            row["review_state"] = "REVIEW_HISTORICAL_REFERENCE_BEFORE_MOVE"
        else:
            row["review_state"] = "MANUAL_REACHABILITY_REVIEW_NOT_SAFE_TO_DELETE"
    top = sorted(rows, key=lambda r:r["bytes"], reverse=True)[:50]
    summary = {
        "schema": "kyoto-full-archive-first-static-review-v1",
        "original_branch": "research/kyoto-decision-coverage",
        "git_head": run(root,["git","rev-parse","HEAD"]).decode().strip(),
        "tracked_files":len(rows),
        "tracked_bytes":sum(r["bytes"] for r in rows),
        "output_tracked_files":sum(r["path"].startswith("output/") for r in rows),
        "output_tracked_bytes":sum(r["bytes"] for r in rows if r["path"].startswith("output/")),
        "by_review_state":dict(Counter(r["review_state"] for r in rows)),
        "archive_state":"NO PRUNING AUTHORIZED; tag must be verified before removals",
        "source_inventory_sha256":hashlib.sha256(
            "".join(x["path"]+"\0"+x["sha256"]+"\n" for x in rows).encode()
        ).hexdigest(),
        "provenance_and_rules":[
            "Every git-tracked file enumerated; no first-400 truncation",
            "Each original tracked file SHA-256 recorded for later external archival",
            "Build/workflow/script/header/doc references examined separately",
            "Absence of a text reference is not proof of runtime unreachability",
            "No original file is removed or moved",
            "Historical negatives and their docs remain valid experimental evidence",
            "No release or Zenodo archive is falsely asserted before verified upload",
            "Do not rewrite history, retune frozen V4 or delete failing tests",
        ],
    }
    return {"summary":summary,"top_50":top,"files":rows}


def write(data: dict, directory: Path):
    directory.mkdir(parents=True, exist_ok=True)
    summary=data["summary"]
    (directory/"FULL_TRACKED_FILES.txt").write_text(
        "\n".join(r["path"] for r in data["files"]) + "\n",
        encoding="utf-8"
    )
    (directory/"FULL_TRACKED_SHA256SUMS.txt").write_text(
        "\n".join(r["sha256"]+"  "+r["path"] for r in data["files"])+"\n",
        encoding="utf-8"
    )
    (directory/"FULL_REACHABILITY_REVIEW.json").write_text(
        json.dumps(data, indent=2) + "\n", encoding="utf-8"
    )
    lines = [
        "# Kyoto archive-first tracked-file reachability audit",
        "",
        "**Read-only audit. No file is automatically declared deletable.**",
        "",
        f'HEAD: {summary["git_head"]}',
        f'Tracked files: **{summary["tracked_files"]}**.',
        f'Tracked bytes: **{summary["tracked_bytes"]:,}**.',
        f'Tracked output files: **{summary["output_tracked_files"]}**.',
        f'Tracked output bytes: **{summary["output_tracked_bytes"]:,}**.',
        "",
        "## Reachability classifications (review states, not delete instructions)",
        ""
    ]
    for status, number in sorted(summary["by_review_state"].items()):
        lines.append(f"- {status}: {number}")
    lines += [
        "",
        "## Top 50 tracked files by byte size",
        "",
        "| Bytes | Path | Review state | CMake | Workflows | Runtime refs | Docs refs |",
        "|---:|---|---|---|---:|---:|---:|"
    ]
    for x in data["top_50"]:
        refs=x["references"]
        lines.append(
            f'| {x["bytes"]:,} | {x["path"]} | {x["review_state"]} '
            f'| {"yes" if refs["cmake_path"] else "no"} '
            f'| {len(refs["workflow_path"])} | {len(refs["runtime_path"])} '
            f'| {len(refs["documentation_path"])} |'
        )
    lines += [
        "",
        "## Original full tracked tree",
        "",
        "The full original 371-entry file inventory is in FULL_TRACKED_FILES.txt;",
        "the complete per-file path/reference/SHA receipts are in",
        "FULL_REACHABILITY_REVIEW.json and FULL_TRACKED_SHA256SUMS.txt.",
        "",
        "## Protected decisions",
        "",
        "All 118 tracked output artifacts are evidence and need verified external",
        "Release/Zenodo transfer before being removed from the active tree.",
        "Source, historical negative-control experiments, tests, launchers,",
        "documentation and CI remain until build/dependency analysis and",
        "review can prove their removal safe.",
        "",
        "No current cleanup deletion has been authorized by this script.",
    ]
    (directory/"FULL_REACHABILITY_REVIEW.md").write_text(
        "\n".join(lines)+"\n",encoding="utf-8"
    )


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    report=audit(args.root.resolve())
    write(report,args.output)
    print(json.dumps(report["summary"], indent=2))


if __name__=="__main__":
    main()
