#!/usr/bin/env python3
"""Phase 17: audit actual 8-byte pair files against persistent-grid fingerprints.

The *producer* is the independent rebuilt-grid Phase 11 streamer. The
comparison is the Phase 16 persistent-grid query on the same procedural
geometry, seed and frame. This script does not claim full solver/CCD fidelity.
"""
from __future__ import annotations

import argparse
import csv
import heapq
from itertools import zip_longest
import json
import struct
import tempfile
from pathlib import Path

MASK64 = (1 << 64) - 1
MASK32 = (1 << 32) - 1
PAIR = struct.Struct("<II")
KEY = struct.Struct("<Q")
FIELDS = ("exact_pairs", "hash64_sum", "hash64_xor", "hash32_sum", "hash32_xor")


def mix64(value: int) -> int:
    value = (value + 0x9E3779B97F4A7C15) & MASK64
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & MASK64
    return value ^ (value >> 31)


def mix32(a: int, b: int) -> int:
    value = (a * 0x9E3779B9 + b * 0x85EBCA6B + 0x165667B1) & MASK32
    value = ((value ^ (value >> 16)) * 0x7FEB352D) & MASK32
    value = ((value ^ (value >> 15)) * 0x846CA68B) & MASK32
    return value ^ (value >> 16)


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        data = list(reader)
    if not data:
        raise ValueError(f"Empty evidence: {path}")
    return data


def sorted_chunk_file(keys: list[int], folder: Path, index: int) -> Path:
    keys.sort()
    path = folder / f"keys-{index:05d}.bin"
    with path.open("wb") as out:
        for value in keys:
            out.write(KEY.pack(value))
    return path


def keys_from_file(path: Path):
    with path.open("rb") as source:
        while block := source.read(8 * 32768):
            if len(block) % KEY.size:
                raise ValueError("Corrupt sorted pair spill")
            for (key,) in struct.iter_unpack("<Q", block):
                yield key


def check_unique(files: list[Path]) -> None:
    # O(K log K) external sorting: no K-sized in-memory set, even for dense
    # frames. Pair IDs are packed into a reversible 64-bit unsigned key.
    prev = None
    for key in heapq.merge(*(keys_from_file(path) for path in files)):
        if key == prev:
            raise ValueError(f"Duplicate materialized pair ID key {key}")
        prev = key


def sorted_binary_keys(path: Path, chunk_pairs: int = 262144):
    """Yield all pair ID keys in sorted order, using bounded-memory disk runs."""
    with tempfile.TemporaryDirectory(prefix="kyoto-pair-compare-") as temp:
        folder = Path(temp)
        files = []
        keys = []
        with path.open("rb") as source:
            while block := source.read(PAIR.size * 32768):
                if len(block) % PAIR.size:
                    raise ValueError("Truncated record while sorting full pair file")
                for a, b in struct.iter_unpack("<II", block):
                    keys.append((a << 32) | b)
                    if len(keys) == chunk_pairs:
                        files.append(sorted_chunk_file(keys, folder, len(files)))
                        keys.clear()
        if keys:
            files.append(sorted_chunk_file(keys, folder, len(files)))
        yield from heapq.merge(*(keys_from_file(item) for item in files))


def compare_exact_pair_files(first: Path, second: Path) -> None:
    if first.resolve() == second.resolve():
        raise ValueError("Independent pair-file reference must be a different file")
    sentinel = -1
    for a, b in zip_longest(sorted_binary_keys(first),
                            sorted_binary_keys(second), fillvalue=sentinel):
        if a != b:
            raise ValueError("Full materialized pair sets are NOT identical")


def audit_binary(path: Path, n: int, unique: bool = True,
                 chunk_pairs: int = 262144) -> tuple[int, ...]:
    size = path.stat().st_size
    if size % PAIR.size:
        raise ValueError(f"Truncated/incomplete pair record: {path}")
    count = sum64 = xor64 = sum32 = xor32 = 0
    with tempfile.TemporaryDirectory(prefix="kyoto-pair-unique-") as temp:
        folder = Path(temp)
        chunks: list[Path] = []
        keys: list[int] = []
        with path.open("rb") as source:
            while block := source.read(PAIR.size * 32768):
                if len(block) % PAIR.size:
                    raise ValueError("Truncated pair block")
                for a, b in struct.iter_unpack("<II", block):
                    if not 0 <= a < b < n:
                        raise ValueError(f"Invalid or noncanonical pair ({a}, {b})")
                    h64 = mix64((a << 32) | b)
                    h32 = mix32(a, b)
                    count += 1
                    sum64 = (sum64 + h64) & MASK64
                    xor64 ^= h64
                    sum32 = (sum32 + h32) & MASK32
                    xor32 ^= h32
                    if unique:
                        keys.append((a << 32) | b)
                        if len(keys) == chunk_pairs:
                            chunks.append(sorted_chunk_file(keys, folder, len(chunks)))
                            keys.clear()
        if unique:
            if keys:
                chunks.append(sorted_chunk_file(keys, folder, len(chunks)))
            check_unique(chunks)
    if size != count * PAIR.size:
        raise ValueError(f"Pair size/count disagreement: {path}")
    return count, sum64, xor64, sum32, xor32


def verify(flat_csv: Path, emitted_csv: Path, unique: bool = True,
           independent_csv: Path | None = None) -> dict:
    flat, emitted = rows(flat_csv), rows(emitted_csv)
    independent = rows(independent_csv) if independent_csv else None
    if independent is not None and len(independent) != len(flat):
        raise ValueError("Independent pair emitter frame count mismatch")
    if len(flat) != len(emitted):
        raise ValueError("Both methods must contain the same frame count")
    records = []
    for idx, (f, e) in enumerate(zip(flat, emitted)):
        for field in ("scene", "n", "frame", "seed"):
            if f[field] != e[field]:
                raise ValueError(f"Frame {idx}: mismatched {field}")
        if int(f["frame"]) != idx:
            raise ValueError("Missing or out-of-order frame")
        if f["status"] != "complete":
            raise ValueError(f"Phase 16 incomplete on frame {idx}")
        if e["status"] != "VERIFIED_WRITTEN" or e["first_count_digest_matches"] != "1":
            raise ValueError(f"Independent pair producer not verified on frame {idx}")
        if e["sink"] != "file":
            raise ValueError("Null-device output does not establish real materialization")
        name = Path(e["output_file"])
        if not name.is_file():
            raise FileNotFoundError(name)
        expected = tuple(int(f[key]) for key in FIELDS)
        observed = audit_binary(name, int(f["n"]), unique=unique)
        if observed != expected:
            raise ValueError(f"Frame {idx}: streamed pairs disagree with persistent digest\n"
                             f"expected={expected}\nobserved={observed}")
        if independent is not None:
            other = independent[idx]
            for key in ("scene", "n", "frame", "seed"):
                if other[key] != f[key]:
                    raise ValueError("Independent producer changed frame geometry")
            if other["status"] != "VERIFIED_WRITTEN":
                raise ValueError("Independent producer did not finish real pair output")
            other_file = Path(other["output_file"])
            if not other_file.is_file():
                raise FileNotFoundError(other_file)
            compare_exact_pair_files(name, other_file)
        if observed[0] != int(e["exact_candidate_pairs"]):
            raise ValueError("Producer count does not match file")
        if observed[0] * PAIR.size != int(e["real_pair_bytes"]):
            raise ValueError("Producer byte accounting does not match file")
        records.append({
            "frame": idx, "pairs": observed[0], "bytes": observed[0] * PAIR.size,
            "serial_or_parallel_backend": f["selected_backend"],
            "phase16_complete_ms": float(f["complete_query_ms"]),
            "rebuilt_pair_stream_ms": float(e["total_stream_ms"]),
            "streamed_pairs_unique_audited": unique,
        })
    return {
        "status": "verified",
        "comparison": "same synthetic input; different rebuilt and persistent implementations",
        "scene": flat[0]["scene"], "n": int(flat[0]["n"]),
        "seed": int(flat[0]["seed"]), "frames": len(records),
        "total_pairs": sum(item["pairs"] for item in records),
        "total_pair_bytes": sum(item["bytes"] for item in records),
        "all_pair_ids_valid": True, "all_pair_digests_equal": True,
        "all_pairs_uniquely_checked": unique,
        "full_pair_list_equal_to_independent_stream": independent is not None,
        "limitations": [
            "AABB candidate list, not narrow phase, CCD or solver contacts",
            "Results from different independent processes; times are not a paired speedup",
            "Hashes alone have collision probability; unique audit checks duplicates only",
            "Source producer's actual candidate predicates remain a separate correctness concern",
        ],
        "records": records,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flat_csv", type=Path)
    parser.add_argument("stream_csv", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--independent-stream-csv", type=Path,
                        help="Compare actual sorted pair IDs against a separate emitter")
    parser.add_argument("--skip-unique-audit", action="store_true",
                        help="Only for gigantic exploratory runs; reported as unverified uniqueness")
    args = parser.parse_args()
    if args.report.exists():
        parser.error(f"Refusing to overwrite existing proof: {args.report}")
    result = verify(args.flat_csv, args.stream_csv,
                    unique=not args.skip_unique_audit,
                    independent_csv=args.independent_stream_csv)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"PASS {result['scene']}: {result['frames']} full pair-file frames, "
          f"{result['total_pairs']:,} pairs, {result['total_pair_bytes']:,} bytes; "
          f"uniqueness-audited={result['all_pairs_uniquely_checked']}")


if __name__ == "__main__":
    main()
