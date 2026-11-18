# Phase 7: Cross-material crowded/contact-wave stress — actual results

Date: 18 November 2026. Branch: research/common-broadphase-orchestrator. Ubuntu GitHub Actions:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44229254090

## What was actually executed

One thousand independently stepped lightweight mass-spring Worlds, including cloth membranes, soft lattices, cushion slabs, elastic balls, and springs, produced 32,600 float32 element AABBs on every recorded frame. Layout `crowded` (seed 45001) packed groups of eight mixed-world instances at close distances. Layout `collision_wave` (seed 45002) drove instances into/out of local inter-instance overlap over 120 frames. Individual Worlds do not exchange collision solver impulses or resolve inter-world penetration: this is a broad-phase candidate test, NOT coupled physical collision simulation.

Each tested CPU method consumed the same immutable Float32 geometry and emitted the exact same full sorted stable-ID candidate pair list as rebuilt SAP on EVERY frame, with occasional independent brute oracle on smaller N in the CI contract. Pair totals were classified by actual World ownership and are not extrapolated from material labels.

## Actual candidate pressure

| Test | Frames | Candidate-pair occurrences (sum across frames) | Inter-world occurrences | Fraction of candidates between different Worlds | First/last-frame inter-world pairs |
|---|---:|---:|---:|---:|---|
| Crowded | 96 | 34,003,508 | 18,346,949 | **53.96%** | 161,333 → 207,630 |
| Moving collision wave | 120 | 25,749,598 | 6,105,275 | **23.71%** | 6,119 → 197,991 |

The contact wave also contained a long stretch of **zero inter-world AABB candidate pairs** during its separated stage, followed by a rise above 200,000 inter-world candidates per frame. This creates a deliberate temporal crossover workload; the trajectory is driven by a designed spatial layout rather than emergent coupled collision physics. Candidate occurrences are frame-repeated observations, not distinct global pair identities.

## Measured full-query CPU wall times on Ubuntu GitHub Actions

| Method | Crowded 1000 Worlds (mean ms) | Contact-wave 1000 Worlds (mean ms) |
|---|---:|---:|
| Frozen V4 | 298.477610 | 166.397308 |
| Rebuilt SAP | 306.490447 | 179.961566 |
| Region exhaustive | 38.920204 | 22.148285 |
| Shared BVH | 53.966780 | 30.199777 |
| **Exact X-swept region** | **34.423314** | **19.190665** |
| Current paid Guard/regional portfolio | 66.060271 | 37.028406 |

Relative to frozen V4, the static X-swept region backend was 8.6708× faster in the crowded case and 8.6707× in the contact-wave case. The controller was 4.52×/4.49× versus V4 but still almost twice as slow as the fixed X-sweep. This is a SINGLE scene/seed per workload on one Ubuntu runner, not multi-seed statistical proof or GPU performance.

### Important research interpretation

These more contact-rich scenes demonstrate that the huge earlier 12–15× V4 wins were NOT solely caused by completely isolated objects; more than half the crowded candidates were across independent Worlds, and X-swept regional remained substantially faster. Nevertheless, other factors remain confounded: the source World budget is ~33 elements per instance, regional ownership is known and stable, and the reference V4 implementation may not have the best algorithmic preprocessing for this workload. Strong published-family incremental SAP and Bullet persistent DBVT controls, matched memory, and full solver-level throughput are still required.

Due to the large win of the fixed backend and the continuing manager penalty, one cannot advertise a novel successful adaptive common broad-phase as already demonstrated. What the evidence currently supports is a useful architecture and a reproducible workload-specific backend crossover, not a final SIGGRAPH/TOG result.

## Reproducible Apple GPU experiment with animated scene during testing

Run from the Mac repository, using a NEW output directory:

    git pull --ff-only origin research/common-broadphase-orchestrator
    SDK="$(xcrun --sdk macosx --show-sdk-path)"
    cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$SDK"
    cmake --build build/kyoto-final-cpu --target ccp_cbp_mass_export ccp_cbp_mass_replay ccp_scaling_metal --parallel 4
    ctest --test-dir build/kyoto-final-cpu -R '^cbp_many_material_worlds_contract$' --output-on-failure
    python3 -m pip install Pillow
    python3 scripts/run_cbp_mass_scale.py --build build/kyoto-final-cpu --output output/cbp-phase7-real-metal-wave --instances 100 1000 --frames 120 --seed 45002 --layout collision_wave --gpu --gif --live-visual --case-timeout 1500

The `--live-visual` option opens a GPU-WebGL geometry-only HTML preview before benchmarks begin, then a measured, hash-verified CPU+Metal dashboard after the final process completes. The `.kmcb` stored geometry is literally shared between CPU and Metal runs. Until an Apple Metal device actually executes this, **there are ZERO measured GPU speedups**; do not transfer Linux CPU numbers into an Apple GPU plot.

Source files: apps/cbp_mass_export.cpp, apps/cbp_mass_replay.cpp, scripts/run_cbp_mass_scale.py, scripts/render_cbp_mass_dashboard.py, viewer/mass_stress.html, .github/workflows/cbp-mass-contact.yml.

Workflow artifact contains raw CPU CSV, experimental metadata, visualization HTML, frame preview, and at least the collision-wave `.kmcb` snapshot stream. Original source and older result files were preserved; the benchmark run refuses accidental output overwrites.

## Before a serious paper claim

Repeat independent seeds within each contact regime with randomized order and low-noise hardware controls. Show true cross-world pair percentages and peak memory. Add comparable actual Bullet DBVT and incremental SAP implementations under the same mask and pair-output semantics, then benchmark total CPU/GPU host time as well as GPU kernel time. Build a truly local hybrid manager choosing methods per actual contact region, charging all cold starts and avoiding future runtime knowledge. Last, integrate multi-World narrow-phase response and inspect genuine surface intersection, self-contact and CCD/solver validity; no GPU/WebGL visualization can substitute for these physical quality tests.
