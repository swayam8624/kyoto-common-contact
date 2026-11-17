# Kyoto soft-material CPU/GPU broad-phase study — runbook

This document accompanies the **experimental** `research/kyoto-scale-evidence` branch. It is written for the next Kyoto faculty discussion. It is not a claim of a complete optimal broad phase, real continuum-material constitutive modeling, or publication-ready scaling proof.

## What is actually being tested

There are **three different experimental layers**. Do not combine or conflate their timings:

1. **Original coupled research simulator:** `MultiCloth::Policy::OnlineContactAware` vs `GlobalGuard`, SAP and regional policies. It applies cloth motion, actual discrete triangle interactions and sphere obstacles. The `ccp_multicloth_benchmark` supports up to 3 cloth sheets with 1–8 spheres. This tests the original professor-facing controller.
2. **Generalized deformable-material candidate workload:** `ccp_material_benchmark` evolves mass-spring cloth, 3D sponge lattice, cushion slab, soft-ball surface, helical spring and a heterogeneous mixed scene. It benchmarks exact padded-AABB candidate generation for SAP rebuild, global guarded reuse, selective local guard repair, fixed/adaptive/hierarchical regions, and `GenericOnline`, which measures cache work and switches between regional search and **selective guarded repair**. This is a generalized controller rather than the original cloth-specific contact feedback. There is NO fully calibrated FEM, volume preservation, gas pressure solver, implicit integrator or CCD, and no claim of precise material self-collision resolution.
3. **Identical serialized CPU/Metal replay:** `ccp_material_export` writes byte-identical Float32 padded AABBs for every frame. `ccp_material_replay` performs seven CPU methods and, when installed, actual upstream persistent Bullet DBVT. `ccp_scaling_metal --replay` performs GPU SAP, fixed regional, and experimental online region-size selection. CPU and Metal must produce the **same pair counts and stable-ID FNV64 hashes** for **every frame**. The GPU online mode adjusts region span using past broad-phase candidate density, not confirmed feature-contact corrections; it is an exploratory GPU policy and not a complete GPU translation of the original online controller.

The original 2026-11-02 global-guard advantage concerns short coupled cloth CPU steps, not GPU or large material datasets. The purpose of these new experiments is to determine whether the advantage persists.

## Materials and physical approximation

| Material fixture | Geometry mapped to AABBs | Dynamics implemented | Missing for physically accurate claims |
| --- | --- | --- | --- |
| Cloth | Two triangles per quadrilateral grid cell | Damped Verlet + structural/shear position constraints + movable spherical impactor | Calibrated textile bending, frictional contacts, continuous self-collision, thickness-dependent constitutive properties |
| Sponge | Volumetric hexahedral lattice cells | Connected 3D springs, soft deformation and sphere impact | Hyperelastic law, volume preservation, porous damping, FEM/MPM |
| Cushion | Flat, thick volumetric slab cells | More strongly damped/compliant 3D springs | Fabric shell + stuffing, realistic viscoelasticity, contact pressure |
| Soft ball | Triangulated closed-looking spherical shell | Radial/structural surface springs and external impact | Pressure, fluid/solid volume coupling, validated rubber elasticity |
| Spring | AABB segments of a helical polyline | Coupled segment springs and anchor endpoints | Torsion, coil diameter, beam/rod bending energy, realistic elastic modulus |
| Mixed | Combination of all five | Interleaved ongoing deformation and overlapping AABBs | True cross-material collision response; the mixed test is **candidate search**, not validated cross-material physics |

All materials have distinct topology, strength and damping; all scenes evolve over time. Static or analytic display meshes are NOT substituted for generated deformable snapshots.

Six deterministic forcing regimes are supported: `baseline`, `impact`, `shear`, `compression`, `rapid`, and `relax`. Use identical seed, regime, primitive budget and frame count for each CPU policy.

## Algorithm and cache comparison

- `sap_rebuild`: exact current-AABB SAP reference, no temporal reuse
- `global_guarded_exact`: a certified **single global** cached envelope pair list; verify containment, rebuild when ANY object escapes or metadata changes, then filter cached superset to CURRENT AABBs
- `selective_guard_exact`: new research engineering alternative, keeps cached pairs unaffected by envelope escapes and repairs pairs for dirty objects against all current envelopes; falls back to full rebuild above a dirty-object threshold; checks exact current AABB overlap
- `region_fixed`, `region_adaptive`, `region_hierarchical`: existing locally certified regional strategies, with separate cache accounting
- `online_cost_aware`: generalized online controller, uses independently measured temporal cost EWMAs, periodic exploration, region size changes and selective-guard fallback; **time includes controller overhead, reindexing, cache repairs and resets**
- `bullet_dbvt_persistent`: actual Bullet DBVT proxy updates when Bullet is installed; uses no substitute implementation. Appears in **serialized CPU replay**, not always in the initial material simulation CSV
- `metal_sap_sorted`, `metal_region_fixed`, `metal_region_online`: real Metal GPU compute kernels; count, exclusive prefix, full pair materialization, sorted stable IDs and pair-hash verification are required.

**Multicore cache repair:** Selective Guard also supports deterministic parallel repair of large dirty-object subsets. Work is partitioned by dirty endpoint across thread-local pair buffers, merged after all workers join, with identical pair-set semantics to the serial path. It falls back to one thread for small workloads to avoid thread-start overhead. The CI regression forces eight workers on 2,048 primitives, verifies full exact-pair equality against serial repair and an independent current-AABB SAP reference, and records `parallel_workers` in the CSV. This parallelism currently applies to **selective guard repair**, not the original cloth-specific regional query, which remains serial; do not imply the entire simulator is parallel.

The new local repair has an independent 280-frame adversarial oracle test with teleports, mask changes and object ID reordering. It passes only if the exact pair set always matches an independently constructed SAP/exhaustive reference. It is an **engineering hypothesis**, not a claim that it is faster than Global Guard in all scenes.

## Quick macOS setup

From an existing checkout with no uncommitted conflicting changes:

```bash
cd /path/to/kyoto-common-contact
git status --short
git fetch origin
git switch research/kyoto-scale-evidence || git switch --track -c research/kyoto-scale-evidence origin/research/kyoto-scale-evidence
git pull --ff-only origin research/kyoto-scale-evidence
brew install cmake glfw bullet
bash scripts/run_kyoto_materials_mac.sh smoke
```

For the full test matrix, including the six material families, forcing-regime variations, original coupled simulator, and byte-for-byte CPU/Metal snapshot replay:

```bash
KYOTO_VERBOSE=2 bash scripts/run_kyoto_materials_mac.sh standard
```

`KYOTO_VERBOSE=1` prints periodic frame progress; `KYOTO_VERBOSE=2` logs each CPU test frame. Use `quick` before `standard` for a smaller run; `stress` explores the expensive upper-end counts. The script is a foreground process. It gives explicit pass/fail results and does **not** hide or rename errors as successes.

All run artifacts are placed under a timestamped `output/kyoto-materials-YYYYMMDDTHHMMSSZ-MODE/` directory. The script **also creates a ZIP** beside that directory, even if an experiment fails. The ZIP includes raw CSVs, frame-level diagnostic logs, failure context, hardware description, CMake and CTest logs, CPU/GPU verifier reports, exact serialized AABB streams, and SHA256 manifests. Upload the ZIP for independent performance analysis.

## macOS Xcode / Anaconda toolchain troubleshooting

On November 15, an Apple Silicon run failed **before any benchmark executed** because the shell invoked Anaconda's CMake 3.28 although Homebrew had installed CMake 4.5.0. Its Objective-C++ compiler test failed at the linker with `ld: library 'c++' not found`. This does not indicate anything about the Online algorithm.

The macOS runner now resolves `brew --prefix` and executes the installed Homebrew CMake/CTest directly. It discovers `clang++` and the macOS SDK via `xcrun`, supplies the SDK explicitly to CMake, and uses `build/kyoto-material-macos` rather than any cached Anaconda-generated `build/CMakeCache.txt`. Before configuration, an Objective-C++ program is compiled **and linked** against that SDK; a failure is preserved in a timestamped diagnostic ZIP instead of continuing with invalid performance results.

After pulling the updated branch, run:

```bash
# Recommended: exit the active "(base)" conda environment if shown.
conda deactivate
git pull --ff-only origin research/kyoto-scale-evidence
KYOTO_VERBOSE=2 bash scripts/run_kyoto_materials_mac.sh smoke
```

Conda deactivation is recommended to reduce unrelated package search pollution, but the script uses explicit CMake paths even if conda remains active.

If the **new** toolchain probe still reports a missing libc++ library, inspect:

```bash
xcode-select -p
xcodebuild -version
xcrun --sdk macosx --show-sdk-path
ls -l "$(xcrun --sdk macosx --show-sdk-path)/usr/lib/libc++.tbd"
/opt/homebrew/opt/cmake/bin/cmake --version
```

If the Xcode path or SDK is truly missing, repair/select the installed full Xcode toolchain before retrying. **Do not** create an empty `libc++.tbd`, insert random dylib symlinks, or suppress linker errors. The new ZIP contains `logs/toolchain-environment.txt` and `logs/toolchain-probe.log` for identifying the exact environment failure.

## Individual commands (after building)

```bash
# All six materials: 512 requested elements each, 100 frames.
./build/ccp_material_benchmark output/experiment-all.csv all 512 100 1234 5 2 baseline

# Same suite with shock-like impact motion:
./build/ccp_material_benchmark output/experiment-impact.csv all 512 100 1234 5 2 impact

# Actual original cloth research, 1/2/3 sheets vs 5 moving spheres:
./build/ccp_multicloth_benchmark output/coupled-five-spheres.csv 80 5

# Export one identical series to replay CPU and GPU:
./build/ccp_material_export output/sponge-impact.kmcb sponge 1024 80 1234 impact
./build/ccp_material_replay output/sponge-impact.kmcb output/sponge-cpu.csv 2
./build/ccp_scaling_metal --replay output/sponge-impact.kmcb output/sponge-gpu-sap.csv sap
./build/ccp_scaling_metal --replay output/sponge-impact.kmcb output/sponge-gpu-region.csv region
./build/ccp_scaling_metal --replay output/sponge-impact.kmcb output/sponge-gpu-online.csv online

python3 scripts/verify_material_cpu_gpu.py output/sponge-cpu.csv output/sponge-gpu-online.csv
```

Do not overwrite a prior CSV; each experiment rejects an existing output. Use unique filenames or the wrapper.

## Correctness and performance controls

- Every material scenario has **actual evolving geometry**, and sampled frames are compared to current-AABB reference pairs. Larger-scene reference uses SAP, while small scenes add an independent exhaustive oracle.
- Serialized material replay replays **identical Float32 AABB bytes**, and **every frame** compares full pair counts and hash across CPU methods. Metal computes the same stable-ID pair sets; a separate validator rejects any hash/count mismatch. This provides strong output agreement but a hash is not a formal mathematical proof against collision.
- Performance measurements distinguish AABB query CPU time, complete coupled cloth CPU timestep, Metal host-end-to-end time and Metal kernel-only execution. Transfer, sorting and pair output are charged to GPU host measurements, never hidden under a kernel-only number.
- All inputs are deterministic and labeled by material, seed, regime, count and source Git SHA. Independent repeats and cross-hardware experiments remain necessary.
- Uncontrolled hardware thermal variation, few repetitions, approximate material dynamics and no tested high-resolution production solver prevent a publication-level claim from these smoke tests alone.
- The offline shader compiler, CMake build and native correctness tests must pass before treating any results as evidence. A hosted macOS CI runner is NOT a substitute for your physical Mac GPU performance measurements.

## What would justify an actual algorithmic win?

Compare online against **global guard, selective guard, persistent Bullet and incremental SAP** at the same exact material snapshot sequence and output semantics. Record when online wins, ties or loses by material, primitive count, motion regime, cache reuse rate, number of local repairs, candidate density, region split decisions and memory. Retune controller costs **only using a training partition**, preserve untouched validation runs and report negative findings.

For the original professor-facing idea, a result from generic AABBs is insufficient alone; we must reproduce the crossover with the actual coupled cloth simulation and eventually with calibrated real soft-body examples. Bigger proxy arrays alone cannot establish material-specific collision-solver performance.
