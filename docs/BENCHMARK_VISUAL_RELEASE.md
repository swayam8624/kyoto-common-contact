# Kyoto final benchmark and visual evidence package

**Status:** Implemented on feature/kyoto-benchmark-visual-cleanup, based on the
validated V4 final release. This is a short, reproducible evidence and
presentation package, **not** another eight-hour controller search.

## What is actually new and executable

* \`apps/visual_multicloth_export.cpp\` independently runs **eight** available
  coupled cloth/sphere solver policies from identical initial states, exports
  every vertex, triangle, sphere and per-frame physics/quality diagnostic as
  JSONL, and **fails** if any policy's positions differ from SAP by >1e-7.
  The final V4 selector is now **explicitly integrated** as the optional
  `Policy::FinalBounded` solver backend. The shared-physics exporter checks
  its geometry against SAP for every frame. This does **not** establish
  faster end-to-end solver performance, nor fewer physical folds.
* \`scripts/render_kyoto_visuals.py solver\` builds a real eight-panel
  synchronized mesh animation showing movements, contact responses,
  candidate counts, strain, near-degenerate triangles and the maximum
  physical-state difference from SAP. The video/GIF loops when supported by
  the player (\`gif\` has loop=0, meaning forever).
* \`scripts/render_kyoto_visuals.py aabb\` builds an automatically looping
  **6 materials x 4 measured approaches** grid cycling through all
  **6 motion regimes** (36 genuine simulated scene sequences). Every
  panel uses the exact **same AABB snapshot and fixed camera** but reads
  its own method's measured CPU latency, candidate counts and pair hashes.
  The visuals sample at most 55 boxes for legibility; candidate correctness
  is still checked for all the original boxes.
* \`benchmarks/import_mesh_sequence.py\` ingests **real** fixed-topology,
  triangulated OBJ frame sequences into the original KYOTOM01 exact AABB
  contract. Refuses topology changes, malformed geometry, missing frames,
  non-finite numbers, or unsupported OBJ face shapes. Writes source and
  output SHA-256 hashes; no external reference data is fabricated.
* \`benchmarks/score_replay.py\` checks each policy's **entire framewise**
  pair-count/hash parity with SAP and rejects methods that miss even
  one pair. Outputs sorted mean/P95 comparative timing tables.
* \`benchmarks/quality_report.py\` measures actual coupled solver
  positional equivalence to SAP, mean edge strain, nearly collapsed
  triangles and recorded self-contact/crossing responses.
* \`benchmarks/compare_reference_geometry.py\` compares time-synchronized
  predicted OBJ mesh surfaces against already-registered PCD/PLY captures.
  Registration is **mandatory and explicit**: it does not align or scale
  data to manufacture low error. The distance is an approximate symmetric
  nearest-neighbor surface RMS, **NOT the official RGBench score**.
* \`CMakePresets.json\` lets you build final/visual targets without rebuilding
  all historical demo executables.
* \`scripts/audit_repository.py\` reports source/CMake reachability and
  flags new generated dependencies; it **does not delete source based
  on grep alone**.

## One-command Mac showcase (short, no model necessary)

\`\`\`bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git worktree add -b kyoto-visual-clean \
  ../kyoto-visual-clean \
  origin/feature/kyoto-benchmark-visual-cleanup
cd ../kyoto-visual-clean

python3 -m pip install Pillow
# Optional: brew install ffmpeg    # adds MP4 alongside GIF
bash scripts/run_kyoto_visual_package.sh output/kyoto-final-showcase
\`\`\`

The runner compiles only the actual exporter, replayer, validated V4 API
and coupled mesh exporter; it runs a short exact-pair smoke test, then
exports 36 material/regime snapshots with 48 frames each at target 128
and records honest method times. Afterward it captures 42 frames from
**each** of the eight coupled mesh policies, verifies exact solver-state
parity, renders GIF/MP4, and writes scores/manifest.

The output is stored **only in the new worktree**, with an
\`output/kyoto-final-showcase/SHOWCASE_MANIFEST.json\` detailing
environment, git SHA and hashes of all source snapshots.
The script refuses to overwrite an existing output directory.

Key outputs:

\`\`\`text
output/kyoto-final-showcase/
  SHOWCASE_MANIFEST.json
  snapshots/                            # 36 real .kmcb fixtures
  replays/                              # per-method measured CSV data
  meshes/                               # 7 verified real-physics JSONL files
  videos/
    all-materials-six-regimes.gif
    all-materials-six-regimes.mp4        # if ffmpeg is installed
    all-eight-solver-policies.gif
    all-eight-solver-policies.mp4        # if ffmpeg is installed
  reports/
    REPLAY_SCORES.md
    REPLAY_SCORES.json
    SOLVER_QUALITY.md
    SOLVER_QUALITY.json
\`\`\`

To increase detail without introducing new controller work:

\`\`\`bash
KYOTO_SHOWCASE_BUDGET=512 KYOTO_SHOWCASE_FRAMES=72 \
  bash scripts/run_kyoto_visual_package.sh output/kyoto-final-showcase-higher-detail
\`\`\`

This is not intended as a statistically independent tournament:
the existing V5/V6 864-cell evidence provides that. Presentation
frames are repeated from genuine snapshots, not fabricated solver motions.

## Render an existing full V5/V6 benchmark directly

The full independent V5/V6 local output directories contain many
methods, with 160-frame geometry snapshots and replay timing CSVs.
Generate an AABB grid **without recomputing any collision algorithm**:

\`\`\`bash
python3 scripts/render_kyoto_visuals.py aabb \
  --run-dir ~/Desktop/Programming/kyoto-unified-v6/output/kyoto-v6-independent-confirmation \
  --budget 512 \
  --methods sap_rebuild global_guarded_exact guard_sap_bounded_trial \
            bvh_refit_exact patch_certificate_fixed unified_certified_hierarchy_v0 \
  --output output/v6-methods-existing-evidence \
  --frames-per-regime 5 --fps 6
\`\`\`

If the raw snapshots/CSVs are not present at that path, the renderer
stops with an explicit error. A JSON summary alone is insufficient
to generate truthful geometry or method-by-frame timing overlays.

## Better external workloads, with source links and limitations

The internal six-material generator and paired AABB snapshots are useful
for **algorithm regression**, but they cannot establish advanced cloth
physics fidelity on their own.

| Benchmark/source | Why it matters | Integration status |
|---|---|---|
| **IPC Toolkit** <https://github.com/ipc-sim/ipc-toolkit> | Optimized, published-family LBVH, spatial hash, contact candidate generation, broad-phase and IPC contact semantics | **External baseline identified; not built into this repo.** IPC face-face/edge-edge filtering, CCD, data layout and multithreading are not interchangeable with our generic AABB-pair API. Do not label our own V5 BVH as IPC LBVH. |
| **ARCSim** <https://graphics.berkeley.edu/resources/ARCSim/index.html> | Cloth/paper/plastic/metal, buckling, folds, anisotropic refinement and stiff materials | Triangulated **fixed-topology OBJ sequences** can be imported now. Adaptive remeshing/tearing requires a different topology-aware adapter; current converter explicitly rejects it. |
| **CLOTH3D** <https://github.com/hbertiche/CLOTH3D> | Large collection of synthetic garment topology/body-motion and garment dynamics | Fixed-topology OBJ sequences can be imported when licensed data is supplied. Original dataset requires registration; no downloads are bundled. |
| **RGBench AAAI 2026** <https://rgbench.github.io/> | Real garment manipulation with measured grasp/fold/fling captures; sim-to-real geometry evaluation | Already aligned PCD/PLY references can be compared with simulated OBJ sequences using the new RMS-neighbor adapter. It is **not** official RGBench evaluation or automatic robot-trajectory simulation. Dataset/camera alignment remain external requirements. |

Official source documentation: IPC Toolkit
<https://github.com/ipc-sim/ipc-toolkit/releases>; ARCSim
<https://graphics.berkeley.edu/resources/ARCSim/index.html>;
CLOTH3D <https://github.com/hbertiche/CLOTH3D>;
RGBench <https://huggingface.co/datasets/RGBench/RGBench-Cloth-Sim2Real-v1>.

### Import a licensed external OBJ animation into the exact replay

\`\`\`bash
python3 benchmarks/import_mesh_sequence.py /path/to/pre_exported/obj_frames \
  --output output/external/frozen-external.kmcb \
  --material cloth --padding 0.0 --seed 9001

cmake --preset kyoto-final-cpu
cmake --build --preset kyoto-final-cpu

./build/kyoto-final-cpu/ccp_material_replay \
  output/external/frozen-external.kmcb \
  output/external/frozen-external.csv 0

python3 benchmarks/score_replay.py \
  output/external/frozen-external.csv \
  --warmup 8 --output output/external/report
\`\`\`

If an external dataset supplies fewer than nine frames, decrease
\`--warmup\`. The importer requires stable vertex/face topology and
does not let the runner claim an external method's score automatically.

### Compare already aligned physical simulation with reference capture

\`\`\`bash
python3 benchmarks/compare_reference_geometry.py \
  --reference-dir /path/to/registered_reference_pcds \
  --simulated-dir /path/to/predicted_mesh_obj_frames \
  --confirm-world-aligned --units meters \
  --max-points 2048 \
  --output output/external/REAL_REFERENCE_GEOMETRY.json
\`\`\`

This requires equivalent frame times and coordinates. If camera
calibration, timestamp synchronization and simulator motion boundary
conditions are unknown, STOP—do not present a sim-to-real score.

## What counts as a real correctness or visual quality improvement

**Broad-phase correctness:** exact full candidate-pair parity, inclusive
AABB overlap, topology-filter coverage, and independently checked
small-case brute-force oracle.

**Physical solver correctness:** contact constraints, residual
penetration after the solver, valid manifold/cloth topology, triangle
intersections, CCD/tunneling, strain and realistic material parameter
calibration. These metrics require additional accurate geometry
oracles and actual solver integration; none are inferred from an
AABB-only replay.

**Visual fidelity:** edge strain, near-collapsed triangles and recorded
self-crossing contact responses are useful **diagnostics**, not a proof
that the cloth is penetration-free. Contact artifacts must be measured
on the same physical solver trajectory or against independently
registered observations, not inferred from the number of broad-phase
pairs returned.

When all exact methods return the same candidates, the physical solver
should produce the same positions. A GIF claiming V4 folds less than
Global Guard on identical paired input would be scientifically dishonest.

## Publication and performance reporting

Report the best V4 score from *independent pre-existing V5/V6
evidence*, keeping geometric speedup separate from summed-time
speedup. Report A/A variability, CPU family, workload size and raw
hashes. Avoid comparing a 36-scene showcase against an 864-scene
holdout as if they were statistically equivalent. Never present an
IPC LBVH speedup until the actual IPC implementation, its
candidate semantics and its cost accounting are integrated.
