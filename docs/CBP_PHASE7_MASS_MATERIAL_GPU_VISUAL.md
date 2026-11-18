# CBP Phase 7 — Many-material CPU, Apple Metal and synchronized visual benchmark

Status: experimental, 18 November 2026. This project is a broad-phase research platform, NOT a finished novel TOG/SIGGRAPH algorithm, physically coupled 1000-material solver, or a continuous-collision guarantee.

## What is actually simulated and measured

The Phase 7 exporter creates 12–1000 INDEPENDENT lightweight mass-spring Worlds, using the existing material_scenes World with real nodes, elastic bonds, and distinct forcing/stiffness/damping for cloth, sponge, cushion, elastic ball and spring families. Every World steps every recorded frame. Its exact element AABBs are placed into a shared 3D coordinate system where collisions between different World instances can occur as BROAD-PHASE AABB candidates. The geometry varies with baseline, compression, shear, impact, rapid and relaxation regimes. Layouts: grid, clusters and contact_front.

Inter-instance contact constraints, narrow-phase forces and self-collision resolution are NOT fed into the separate mass-spring Worlds. The video depicts actual instance envelopes, not solved inter-material deformations. Do not claim improved folding or intersection avoidance.

Immutable shared input: snapshots.kmcb uses KYOTOM01 mixed-material Float32 element AABBs. instances.csv records material types and exact stable primitive ownership. preview.jsonl records per-frame actual geometry envelopes for WebGL2/GIF. frames.csv records frame identities and geometry fingerprints.

CPU tournament: ccp_cbp_mass_replay. The default core policy runs 6 independent algorithms: rebuilt SAP, frozen V4, regional exhaustive, regional X-sweep, shared BVH, and charged V4 portfolio. The opt-in full policy adds the certified graph and causal CBP when total element AABBs <=16000. Each algorithm consumes THE SAME stored Float32 coordinates; every frame's full sorted pair list is compared with SAP; selected small frames use independent brute-force checks. Candidate pairs above 16 million are refused.

Apple Metal: ccp_scaling_metal --replay reads the identical binary for GPU SAP, GPU region or GPU online-region. The size limit for replay was increased to 65536 elements while preserving the 16 million materialized-pair cap. The GPU region and online-region algorithms use their existing contiguous-block partition, NOT the CPU's material-instance ownership. These are distinct algorithms. The renderer joins CPU and GPU only if every frame's exact pair count and full stable-ID FNV hash agree. GPU shader times and whole GPU host wall times are recorded separately.

Visualization: viewer/mass_stress.html uses WebGL2 instanced line boxes for every stepped material-instance envelope; not a still image. It displays five type colors, live frame seek, play/pause, FPS, orbit, zoom, type filter and actual per-frame benchmark timings. Optional Pillow loops create a measured 2D GIF. The live-visual runner can open a GEOMETRY-ONLY preview as the benchmark executes, with no fabricated timing or correctness claims, then opens the independently verified final dashboard.

## Completed 100/1000-world Linux CI result

GitHub Actions execution: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44227446215

One contact_front layout, seed 42017, 48 frames, exact full CPU pair parity, Ubuntu runner. 100 Worlds generated 3260 element AABBs; 1000 Worlds generated 32600 element AABBs.

| Worlds | Fixed frozen V4 mean ms | Regional exhaustive ms | Shared BVH ms | X-swept region ms | V4+regional portfolio ms |
|---|---:|---:|---:|---:|---:|
| 100 | 2.996936 | 0.848119 | 1.261445 | 0.811800 | 1.138954 |
| 1000 | 120.547146 | 11.525213 | 13.944805 | 8.255367 | 26.329416 |

The 1000-world X-swept reference is 14.60x faster than frozen V4 **on this one synthetic-generator scene**. This is NOT a publication result, solver speedup, or independently randomized held-out evaluation. The runner logged no Apple Metal data (0 GPU results); GPU speedups must not be invented. The Github workflow preserves raw binary snapshots, CPU CSV, WebGL HTML and SHA manifest in an artifact.

The controller still trails the strongest fixed regional implementation. Its switching and expensive default Guard stages add significant overhead; the correct scientific goal is a causal, low-overhead, truly region-local manager, not simply a fast frozen backend.

## MAC: run actual Metal and watch geometry during the test

Build and verify:

    cd ~/Desktop/Programming/kyoto-common-contact
    git pull --ff-only origin research/common-broadphase-orchestrator
    SDK="$(xcrun --sdk macosx --show-sdk-path)"
    cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$SDK"
    cmake --build build/kyoto-final-cpu --target ccp_cbp_mass_export ccp_cbp_mass_replay ccp_scaling_metal --parallel 4
    ctest --test-dir build/kyoto-final-cpu -R '^cbp_many_material_worlds_contract$' --output-on-failure

For a real many-minute run including Apple Metal and a looping GIF (requires Pillow):

    python3 -m pip install Pillow
    python3 scripts/run_cbp_mass_scale.py --build build/kyoto-final-cpu --output output/cbp-mass-mac-real-gpu --instances 100 1000 --frames 120 --seed 43129 --layout contact_front --gpu --gif --live-visual --case-timeout 1500

The live-visual option opens actual recorded WebGL scene geometry BEFORE benchmarking, marked as unmeasured. The final viewer is opened when full CPU/Metal exact pair hash checks pass. No evidence path is overwritten.

If GPU execution fails or the output cap is reached, do not silently retry with a different algorithm under the same report name. Record the failure and adjust the workload in a fresh output folder.

## Research next gate

Run scene-layout/seed controls (grid, clusters, contact_front), 1000 Worlds x 120–240 frames, warmup excluded, repeated runs, power/thermal and memory accounting, and independent hardware validation. Compare strong production incremental SAP and Bullet persistent DBVT baselines when their input semantics match. Next build an actual cross-instance narrow-phase/contact solver and mesh-based visual, then measure complete solver step, physical error, stability, CCD, and contact quality. Only a robust, truly adaptive local maintenance method that survives held-out workloads could qualify as a candidate research contribution.
