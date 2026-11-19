#!/usr/bin/env python3
"""Run a bounded, reproducible, threshold-eligible Kyoto V4/Online comparison.

This is an ADDITIVE on-policy experiment: original Global Guard, V4,
generalized Online, SAP, selective Guard, regional, optional trained ML,
optional real Bullet. The frozen production Guard/SAP controller is NOT
modified. A separate rigid-translation diagnostic is deliberately artificial:
it forces large envelope displacements while preserving relative geometry,
and is NEVER pooled with physically stepped material trajectories.

Every method receives the exact same saved KYOTOM01 snapshot sequence.
Each trajectory randomizes execution order with an explicit immutable seed.
Untimed independent SAP builds expected pairs before each shuffled run.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_kyoto_phase17_decisions import audit as audit_decisions
from kyoto_research_dashboard import material_results

HEADER = struct.Struct("<8sIIII")
BOX = struct.Struct("<ffffff")
MIN_ACTIVATION_SIZE = 384
VALID_MATERIALS = {"cloth", "sponge", "cushion", "soft_ball", "spring", "mixed"}
VALID_REGIMES = {"baseline", "compression", "shear", "impact", "rapid", "relax"}


def sha256(file: Path) -> str:
    digest = hashlib.sha256()
    with file.open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def snapshot_metadata(path: Path) -> dict:
    with path.open("rb") as source:
        raw = source.read(HEADER.size)
    if len(raw) != HEADER.size:
        raise ValueError(f"Truncated snapshot header: {path}")
    magic, n, frames, seed, material = HEADER.unpack(raw)
    if magic != b"KYOTOM01" or not 1 <= n <= 50000 or not 1 <= frames <= 10000:
        raise ValueError(f"Invalid immutable material header: {path}")
    if path.stat().st_size != HEADER.size + n * frames * BOX.size:
        raise ValueError(f"Truncated or oversized snapshot body: {path}")
    return {"n": n, "frames": frames, "seed": seed, "material_code": material}


def random_order_seed(*parts: object) -> int:
    # Deterministic even across processes/platforms, unlike hash().
    raw = "|".join(str(part) for part in parts).encode("utf-8")
    return int.from_bytes(hashlib.sha256(raw).digest()[:4], "little") or 1


def translated_diagnostic(source: Path, destination: Path) -> None:
    """Artificial stress, never presented as new material physics.

    Translate the WHOLE current scene alternately by 0.30 world units along
    all 3 axes. Relative positions and AABB candidate sets remain the same,
    but guard envelopes with margin 0.045 have to be re-certified repeatedly.
    This tests black-box V4 trigger eligibility without changing thresholds.
    """
    if destination.exists():
        raise FileExistsError(destination)
    meta = snapshot_metadata(source)
    with source.open("rb") as input_stream, destination.open("xb") as output:
        output.write(input_stream.read(HEADER.size))
        for frame in range(meta["frames"]):
            displacement = .30 if frame % 2 else 0.
            for _ in range(meta["n"]):
                raw = input_stream.read(BOX.size)
                if len(raw) != BOX.size:
                    raise ValueError("Unexpected end of diagnostic input")
                a, b, c, x, y, z = BOX.unpack(raw)
                output.write(BOX.pack(
                    a + displacement, b + displacement, c + displacement,
                    x + displacement, y + displacement, z + displacement))
        if input_stream.read(1):
            raise ValueError("Material snapshot has unexpected trailing bytes")


def run_command(cmd: list[str], log: Path, timeout: int) -> None:
    if log.exists():
        raise FileExistsError(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("x", encoding="utf-8") as output:
        output.write("COMMAND: " + " ".join(cmd) + "\n\n")
        output.flush()
        try:
            with subprocess.Popen(
                cmd, stdout=output, stderr=subprocess.STDOUT, text=True
            ) as proc:
                proc.wait(timeout=timeout)
                code = proc.returncode
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            raise RuntimeError(f"Timed out; see {log}") from None
    if code != 0:
        raise RuntimeError(f"Process exited {code}; see {log}")


def check_ordered_replay(path: Path, seed: int) -> dict:
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    methods = sorted({row["backend"] for row in rows})
    slots: dict[str, set[int]] = defaultdict(set)
    if not rows:
        raise ValueError(f"Empty replay: {path}")
    for row in rows:
        if int(row["order_seed"]) != seed:
            raise ValueError(f"Wrong policy execution order seed in {path}")
        slots[row["backend"]].add(int(row["execution_slot"]))
    if any(len(slots[name]) != 1 for name in methods):
        raise ValueError(f"Policy execution slot changed during a trajectory: {path}")
    observed = {name: next(iter(slots[name])) for name in methods}
    if set(observed.values()) != set(range(len(methods))):
        raise ValueError(f"Missing or duplicated execution-order slot: {path}")
    return observed


def build_summary(root: Path, warmup: int, cases: list[dict]) -> dict:
    # Exact-pair scorer and eligibility audit refuse malformed records.
    material = material_results(root, warmup)
    decisions = audit_decisions(root, warmup)
    if len(material["files"]) != len(cases) or len(decisions["scenes"]) != len(cases):
        raise ValueError("Missing raw replay or a skipped material trajectory")
    classifications = {case["csv"]: case["type"] for case in cases}
    by_scene = []
    grouped = defaultdict(list)
    action_by_file = {str(Path(x["file"])): x for x in decisions["scenes"]}
    for case in cases:
        timing = action_by_file.get(case["csv"])
        if timing is None:
            raise ValueError(f"Cannot locate decision audit for {case['csv']}")
        item = {
            "type": case["type"], "material": case["material"],
            "regime": case["regime"], "actual_n": case["actual_n"],
            "seed": case["seed"], "order_seed": case["order_seed"],
            "guard_over_v4": timing["guard_over_v4_excluding_warmup"],
            "v4_guard_frames": timing["v4_backend_frames"].get("global_guarded_exact", 0),
            "v4_sap_frames": timing["v4_backend_frames"].get("sap_persistent_coherent", 0),
            "trial_frames": timing["v4_trial_frames"],
            "switches": timing["v4_switches"],
            "csv": case["csv"],
        }
        by_scene.append(item)
        grouped[case["type"]].append(item)
    report = {
        "experiment": "Phase 18 controller-activation coverage, no threshold retuning",
        "warmup": warmup,
        "tested_methods": sorted(material["methods"]),
        "ml_trained_model_supplied": decisions["learned_model_executed"],
        "bullet_executed": decisions["bullet_executed"],
        "natural_and_artificial_diagnostic_never_pooled": True,
        "data": cases,
        "results": by_scene,
        "groups": {
            cohort: {
                "trajectories": len(items),
                "guard_frames": sum(x["v4_guard_frames"] for x in items),
                "sap_frames": sum(x["v4_sap_frames"] for x in items),
                "trial_frames": sum(x["trial_frames"] for x in items),
                "switches": sum(x["switches"] for x in items),
                "observed_eligible_scenes": sum(x["actual_n"] >= MIN_ACTIVATION_SIZE for x in items),
            }
            for cohort, items in sorted(grouped.items())
        },
        "limitations": [
            "Size eligibility does not guarantee expensive repeated Guard rebuilds or an SAP trial",
            "Diagnostic rigid translations are mathematically shifted snapshots, not newly solved physics",
            "CPU current AABB broad phase only; no CCD, narrow phase or full solver speedup",
            "Single-pass ordered policies can retain thermal drift despite seeded order randomization",
            "Only trained independently held-out ML models may support a learned-policy efficacy claim",
            "No automatic threshold retuning or per-frame hindsight oracle",
            "Independent random seeds are repeated trajectories; adjacent frames are correlated",
        ],
    }
    return report


def write_summary(result: dict, output: Path) -> None:
    report = output / "DECISION_COVERAGE.json"
    report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Kyoto V4 and original Online: real action coverage", "",
        "The original frozen V4 code is unchanged. Only exact, immutable "
        "full-output material replay CSVs enter this report.", "",
        "**The artificial rigid-translation diagnostic is NOT a physically "
        "solved material workload and must not be pooled with natural dynamics.**", "",
        f"Executed algorithms: {', '.join(result['tested_methods'])}", "",
        f"Trained ML file supplied and executed: **{result['ml_trained_model_supplied']}**. "
        f"Bullet executed: **{result['bullet_executed']}**.", "",
        "| Cohort | Trajectories | Size eligible | Guard frames | SAP frames | Trial frames | Switches |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for name, values in result["groups"].items():
        lines.append(
            f"| {name} | {values['trajectories']} | {values['observed_eligible_scenes']} | "
            f"{values['guard_frames']} | {values['sap_frames']} | "
            f"{values['trial_frames']} | {values['switches']} |")
    lines.extend([
        "", "## Individual full-output comparisons", "",
        "| Cohort | Material | Regime | Actual N | Seed | Guard/V4 warm | SAP frames | Switches |",
        "|---|---|---|---:|---:|---:|---:|---:|",
    ])
    for item in result["results"]:
        lines.append(
            f"| {item['type']} | {item['material']} | {item['regime']} | "
            f"{item['actual_n']} | {item['seed']} | "
            f"{item['guard_over_v4']:.4f}x | {item['v4_sap_frames']} | "
            f"{item['switches']} |")
    lines.extend(["", "## Boundaries", ""])
    lines.extend("- " + limitation for limitation in result["limitations"])
    (output / "DECISION_COVERAGE.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, required=True)
    parser.add_argument("--replayer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--model", type=Path,
                        help="Original independently trained native .mlpc only; omitted means ML not run")
    parser.add_argument("--materials", nargs="+", default=["cloth", "sponge", "mixed"])
    parser.add_argument("--regimes", nargs="+", default=["baseline", "rapid"])
    parser.add_argument("--budgets", nargs="+", type=int, default=[512, 1024])
    parser.add_argument("--seeds", nargs="+", type=int, default=[3203, 3217, 3251])
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--timeout-seconds", type=int, default=600)
    parser.add_argument("--skip-translation-diagnostic", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        parser.error(f"Refusing to replace experiment output: {args.output}")
    if any(mat not in VALID_MATERIALS for mat in args.materials):
        parser.error("Unknown material name")
    if any(regime not in VALID_REGIMES for regime in args.regimes):
        parser.error("Unknown regime")
    if any(n < 384 or n > 2048 for n in args.budgets):
        parser.error("Material requested budgets must fall in 384..2048")
    if any(seed < 1 or seed > 2000000000 for seed in args.seeds):
        parser.error("Invalid deterministic material seed")
    if not 10 <= args.frames <= 240 or not 0 <= args.warmup < args.frames:
        parser.error("Invalid frame or warmup bound")
    if len(set(args.materials)) != len(args.materials) or \
       len(set(args.regimes)) != len(args.regimes) or \
       len(set(args.budgets)) != len(args.budgets) or \
       len(set(args.seeds)) != len(args.seeds):
        parser.error("Duplicate experiment matrix entries")
    if args.model and not args.model.is_file():
        parser.error(f"Trained model file not found: {args.model}")
    exporter = str(args.exporter.resolve())
    replayer = str(args.replayer.resolve())
    model = str(args.model.resolve()) if args.model else ""
    if not Path(exporter).is_file() or not Path(replayer).is_file():
        parser.error("Native exporter and replayer must exist and be built")
    # Fail before creating an output folder when the wrong older binary is supplied.
    capability = subprocess.run([replayer, "--capabilities"], capture_output=True,
                                text=True, check=False, timeout=20)
    if capability.returncode or "guard_sap_bounded_trial" not in capability.stdout:
        parser.error("Wrong or stale material replayer binary")
    args.output.mkdir(parents=True, exist_ok=False)
    for subdir in ("snapshots", "replays", "logs"):
        (args.output / subdir).mkdir()
    cases = []
    total_natural = len(args.materials) * len(args.regimes) * \
        len(args.budgets) * len(args.seeds)
    print(f"Beginning {total_natural} natural material trajectories "
          f"with {args.frames} frames each; output {args.output}", flush=True)

    def execute_case(snapshot: Path, material: str, regime: str,
                     budget: int, seed: int, classification: str) -> None:
        meta = snapshot_metadata(snapshot)
        if meta["n"] < MIN_ACTIVATION_SIZE:
            raise ValueError(
                f"Actual generated N={meta['n']} under V4 trial floor. "
                f"Requested budget={budget}, material={material}; refuse invalid inference.")
        if meta["frames"] != args.frames or meta["seed"] != seed:
            raise ValueError("Exported scene header disagrees with requested trial")
        order = random_order_seed(material, regime, budget, seed, classification)
        run_label = f"{material}-{regime}-requested{budget}-actual{meta['n']}-seed{seed}-{classification}"
        csv_path = args.output / "replays" / f"{run_label}-evaluated.csv"
        command = [replayer, str(snapshot), str(csv_path), "0"]
        if model:
            command.append(model)
        command += ["--order-seed", str(order)]
        run_command(command, args.output / "logs" / f"{run_label}-replay.log",
                    args.timeout_seconds)
        order_map = check_ordered_replay(csv_path, order)
        cases.append({
            "type": classification, "material": material, "regime": regime,
            "budget_requested": budget, "actual_n": meta["n"],
            "seed": seed, "frames": args.frames, "order_seed": order,
            "execution_order": order_map,
            "snapshot": str(snapshot), "snapshot_sha256": sha256(snapshot),
            "csv": str(csv_path), "csv_sha256": sha256(csv_path),
        })
        print(f"PASS {classification}: {material} {regime}, "
              f"requested={budget}, actual N={meta['n']}, seed={seed}, "
              f"order={order}", flush=True)

    for budget in args.budgets:
        for material in args.materials:
            for regime in args.regimes:
                for seed in args.seeds:
                    stem = f"{material}-{regime}-requested{budget}-seed{seed}"
                    snapshot = args.output / "snapshots" / f"{stem}.kmcb"
                    run_command(
                        [exporter, str(snapshot), material, str(budget),
                         str(args.frames), str(seed), regime],
                        args.output / "logs" / f"{stem}-export.log",
                        args.timeout_seconds)
                    execute_case(snapshot, material, regime, budget, seed, "natural")

    if not args.skip_translation_diagnostic:
        # One artificial stress source PER seed. Do not bias the natural
        # selection score by adding these diagnostic sequences as replicates.
        budget = max(args.budgets)
        material, regime = args.materials[0], "rapid"
        for seed in args.seeds:
            stem = f"translation-diagnostic-{material}-requested{budget}-seed{seed}"
            original = args.output / "snapshots" / f"{stem}-base.kmcb"
            moved = args.output / "snapshots" / f"{stem}-translated.kmcb"
            run_command(
                [exporter, str(original), material, str(budget),
                 str(args.frames), str(seed), regime],
                args.output / "logs" / f"{stem}-export.log",
                args.timeout_seconds)
            translated_diagnostic(original, moved)
            execute_case(moved, material, regime, budget, seed,
                         "rigid_translation_diagnostic")

    result = build_summary(args.output, args.warmup, cases)
    write_summary(result, args.output)
    print(f"PASS {len(cases)} distinct, random-ordered trajectories. "
          f"V4 choices: " + json.dumps(result["groups"], sort_keys=True), flush=True)
    print(f"Read {args.output / 'DECISION_COVERAGE.md'}", flush=True)


if __name__ == "__main__":
    main()
