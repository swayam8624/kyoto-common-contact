#!/usr/bin/env python3
"""Package all available Kyoto run evidence, even after a failed test."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile
from datetime import datetime, timezone

def digest(file):
    hasher=hashlib.sha256()
    with file.open("rb") as input:
        for chunk in iter(lambda: input.read(1024*1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir",type=Path)
    parser.add_argument("--exit-code",type=int,default=0)
    parser.add_argument("--diagnostics-only",action="store_true",
                        help="Small interruption ZIP: logs, model trials, and metadata only; leave large snapshots intact")
    args=parser.parse_args()
    root=args.run_dir
    if not root.is_dir():
        raise SystemExit(f"Missing Kyoto run directory: {root}")
    source=sorted(p for p in root.rglob("*") if p.is_file()
                  and p.name not in {"SHA256_MANIFEST.json","RUN_STATUS.json"})
    if args.diagnostics_only:
        # A 1080-scene exhaustive run can occupy gigabytes. Do not spend
        # minutes hashing/compressing immutable geometry simply because
        # the user interrupted training. Those original files remain in place.
        source=[p for p in source if p.relative_to(root).parts[0]
                not in {"teacher","snapshots","heldout"}]
    manifest={"directory":str(root),
              "created_utc":datetime.now(timezone.utc).isoformat(),
              "exit_code":args.exit_code,
              "status":"complete" if args.exit_code==0 else "failed_or_partial",
              "artifact_scope":"diagnostics_only" if args.diagnostics_only else "full",
              "excluded_dirs":(["teacher","snapshots","heldout"]
                               if args.diagnostics_only else []),
              "original_files_are_untouched":True,
              "files":{}}
    for p in source:
        manifest["files"][str(p.relative_to(root))]={
            "size_bytes":p.stat().st_size,"sha256":digest(p)}
    if not args.diagnostics_only:
        (root/"SHA256_MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n")
        (root/"RUN_STATUS.json").write_text(
            json.dumps({"exit_code":args.exit_code,"status":manifest["status"]},indent=2)+"\n")
        source=sorted(p for p in root.rglob("*") if p.is_file())
        archive=root.with_suffix(".zip")
    else:
        # Produce a new immutable diagnostics archive on each partial exit.
        stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        archive=root.with_name(root.name+"-partial-"+stamp+".zip")
    if archive.exists():raise SystemExit("Refusing to overwrite prior research archive")
    with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED,compresslevel=5,allowZip64=True) as zip:
        if args.diagnostics_only:
            zip.writestr(str(root.name)+"/DIAGNOSTIC_MANIFEST.json",
                         json.dumps(manifest,indent=2)+"\n")
        for index,p in enumerate(source,1):
            if p.is_file():
                zip.write(p,p.relative_to(root.parent))
            if index%100==0:
                print(f"[KYOTO] packaged {index}/{len(source)} files",flush=True)
    print(f"[KYOTO] ARCHIVE: {archive}",flush=True)
    if args.diagnostics_only:
        print("[KYOTO] Partial diagnostics ZIP excludes large snapshots/CSV; "
              "original complete data remain in the run folder for resume.",flush=True)
    else:
        print("[KYOTO] Full ZIP includes original experiment data and logs.",flush=True)

if __name__=="__main__":
    main()
