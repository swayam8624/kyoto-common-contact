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
    args=parser.parse_args()
    root=args.run_dir
    if not root.is_dir():
        raise SystemExit(f"Missing Kyoto run directory: {root}")
    source=sorted(p for p in root.rglob("*") if p.is_file()
                  and p.name not in {"SHA256_MANIFEST.json","RUN_STATUS.json"})
    manifest={"directory":str(root),
              "created_utc":datetime.now(timezone.utc).isoformat(),
              "exit_code":args.exit_code,
              "status":"complete" if args.exit_code==0 else "failed_or_partial",
              "files":{}}
    for p in source:
        manifest["files"][str(p.relative_to(root))]={
            "size_bytes":p.stat().st_size,"sha256":digest(p)}
    (root/"SHA256_MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n")
    (root/"RUN_STATUS.json").write_text(
        json.dumps({"exit_code":args.exit_code,"status":manifest["status"]},indent=2)+"\n")
    archive=root.with_suffix(".zip")
    if archive.exists():raise SystemExit("Refusing to overwrite prior research archive")
    with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED,compresslevel=5,allowZip64=True) as zip:
        for p in sorted(root.rglob("*")):
            if p.is_file():zip.write(p,p.relative_to(root.parent))
    print(f"[KYOTO] ARCHIVE: {archive}",flush=True)
    print("[KYOTO] Upload this ZIP even if the run failed. Logs and partial CSVs are retained.",flush=True)

if __name__=="__main__":
    main()
