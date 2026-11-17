#!/usr/bin/env python3
"""Static, non-destructive Kyoto repository hygiene and reachability audit.

No false 'unused' declarations from grep alone: C++ source files referenced
by CMake are "build-referenced"; others are "needs human review", never
deleted automatically. Runtime-only components, dynamic includes and
runtime-loaded resources can escape static scans. Existing source branches
preserve the 2026-11-17 experiment history.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import Counter
from pathlib import Path

ORPHAN_PATTERNS=(
    ".presentation-3d/",".presentation-native/",".presentation-rebuild/",
    ".presentation-research/",".render3d/",".presentation-refine-",
    ".presentation-review-","output/research3d/video_frames/",
)
GENERATED_SUFFIXES=(".ppm",".pyc",".blend1")
SOURCE_EXT={".cpp",".cxx",".cc",".mm",".m",".hpp",".h"}

def audited(root):
    proc=subprocess.run(["git","ls-files","-z"],cwd=root,
                        capture_output=True,check=True)
    paths=[Path(p.decode("utf-8")) for p in proc.stdout.split(b"\x00") if p]
    paths.sort()
    manifest=(root/"CMakeLists.txt").read_text(encoding="utf-8")
    source_text=""
    for p in paths:
        if p.suffix in SOURCE_EXT and (root/p).is_file():
            try:source_text+=(root/p).read_text(encoding="utf-8")+"\n"
            except UnicodeDecodeError:pass
    offenders=[];size=0
    source_status=[]
    for p in paths:
        file=root/p
        if not file.is_file():continue
        n=file.stat().st_size;size+=n
        name=p.as_posix()
        if any(name.startswith(pattern) for pattern in ORPHAN_PATTERNS):
            offenders.append({"path":name,"reason":"generated/vendor presentation cache"})
        if n>8_000_000:
            offenders.append({"path":name,"reason":"larger than 8 MB in source control",
                              "bytes":n})
        if name.startswith("output/native/") and p.suffix==".ppm":
            offenders.append({"path":name,"reason":"raw uncompressed framebuffer"})
        if p.suffix in SOURCE_EXT:
            if p.suffix in {".cpp",".cxx",".cc",".mm",".m"}:
                if name in manifest:
                    status="CMake target / build referenced"
                elif p.name in source_text.replace(name,""):
                    status="named in C++ source; review transitive use"
                else:
                    status="no direct CMake reference; manually review"
            else:
                # Include directives only—not proof of runtime use.
                count=len(re.findall(r'#\s*include\s*["<][^">]*'+
                                     re.escape(p.name)+r'[">]',source_text))
                status=(f"direct C++ include references: {count}" if count
                        else "header not directly included; manually review")
            source_status.append({"path":name,"status":status,
                                  "bytes":n})
    return {
        "schema":1,"files_tracked":len(paths),"tracked_megabytes":round(size/1e6,3),
        "by_top_level":dict(Counter(p.parts[0] for p in paths)),
        "generated_or_oversized":offenders,
        "source_reference_audit":source_status,
        "methodology":"static reachability only; no automatic source deletion or speedup claim",
        "protected":[
            "include/ccp/final_broadphase.hpp",
            "src/research_multicloth.cpp",
            "src/online_partitioner.cpp",
            "apps/material_replay_benchmark.cpp",
            "apps/material_snapshot_export.cpp",
            "tests/final_broadphase_api_tests.cpp",
            "DEVLOG.md",
            "FINAL_RELEASE.md",
            "output/review-2026-11-02/RESULTS.md",
        ]
    }

def write(data,directory):
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    (directory/"REPO_AUDIT.json").write_text(json.dumps(data,indent=2)+"\n")
    lines=[
        "# Kyoto source/asset audit","",
        f'**Tracked files:** {data["files_tracked"]}; **tracked size:** {data["tracked_megabytes"]:.1f} MB.',
        "",
        "## Generated/large files needing review",
        "",
    ]
    if not data["generated_or_oversized"]:lines.append("No known vendored-cache or >8 MB tracked-file regressions.")
    else:
        for obj in data["generated_or_oversized"]:
            lines.append(f'- \`{obj["path"]}\` — {obj["reason"]}')
    lines+=["","## CMake/source reachability (static, NOT proof of dead code)","",
            "| Source path | Observed status |","|---|---|"]
    for x in data["source_reference_audit"]:
        lines.append(f'| \`{x["path"]}\` | {x["status"]} |')
    lines.extend(["","## Engineering policy","",
                  "- Historical experiments are preserved on old Git branches.",
                  "- Production V4 source and the coupled solver are protected.",
                  "- Do not delete a file because text search alone cannot find its caller.",
                  "- Never claim faster hot-path runtime without a timed, exact-oracle regression.",
                  "- Build only requested Release CMake targets via CMakePresets.",
                  "- Generated videos, binary dependencies and massive raw captures stay ignored.",""])
    (directory/"REPO_AUDIT.md").write_text("\n".join(lines)+"\n")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root",type=Path,default=Path(__file__).resolve().parent.parent)
    p.add_argument("--output",type=Path)
    p.add_argument("--fail-on-generated",action="store_true")
    a=p.parse_args()
    result=audited(a.root.resolve())
    if a.output:write(result,a.output)
    print(f'{result["files_tracked"]} tracked files, '
          f'{result["tracked_megabytes"]:.2f} MB, '
          f'{len(result["generated_or_oversized"])} generated/oversized flags')
    for item in result["generated_or_oversized"][:15]:
        print("FLAG",item["path"],item["reason"])
    if a.fail_on_generated and result["generated_or_oversized"]:
        raise SystemExit("Repository hygiene check failed")

if __name__=="__main__":main()
