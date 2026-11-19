# Research Observatory — native numbers, tables and 3D visual analytics

The five-policy cloth proof is a limited physics-validation scene, not a
multi-material benchmarking dashboard. We now keep those evidence paths separate.

## 1. Update and build (Mac)

~~~bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/reach-budget-calculus
git pull --ff-only origin research/reach-budget-calculus
cmake --preset kyoto-final-cpu
cmake --build build/kyoto-final-cpu --parallel 4 \
  --target ccp_material_export ccp_rbc_material_tournament \
           ccp_rbc_coupled_tournament ccp_rbc_physical_geometry_tests
ctest --test-dir build/kyoto-final-cpu -R 'rbc_' --output-on-failure
~~~

## 2. Run a real multi-material pilot

This intentionally limited pilot uses 6 material families (cloth, sponge,
cushion, soft ball, spring, mixed) x 2 regimes (rapid, shear) x 3 budgets
(128, 512, 1024) x 1 independent source variant = **36 native material
scenes**, plus 3 coupled-solver scenarios. It is exploratory, not statistically
conclusive. Use a fresh output folder; the gate never overwrites original data.

~~~bash
python3 -u scripts/run_rbc_research_gate.py \
  --exporter build/kyoto-final-cpu/ccp_material_export \
  --replayer build/kyoto-final-cpu/ccp_rbc_material_tournament \
  --coupled build/kyoto-final-cpu/ccp_rbc_coupled_tournament \
  --output output/rbc-observatory-native-pilot-01 \
  --regimes rapid shear --budgets 128 512 1024 \
  --variants 1 --frames 60 --warmup 8 \
  --coupled-variants 1 --coupled-frames 60 --audit-coupled 0 \
  | tee output/rbc-observatory-native-pilot-01.console.txt

python3 -u scripts/rbc_benchmark_observatory.py \
  --research output/rbc-observatory-native-pilot-01/RBC_RESULTS.json \
  --output output/rbc-observatory-dashboard-01 \
  | tee output/rbc-observatory-dashboard-01.console.txt

open output/rbc-observatory-dashboard-01/RESEARCH_OBSERVATORY.html
~~~

The observatory independently checks native source SHA-256 receipts,
per-frame pair count and fingerprints, recalculated query means and
coupled whole-step means. It prints leaderboard and whole-solver tables
IN THE TERMINAL and writes an audit CSV and a fully offline interactive
HTML containing a material/budget heatmap and a rotatable 3D speedup surface.
A speedup is only relative to Guard on the same native scene; it is **not**
a full solver speedup. Empty cells remain labeled unmeasured.

## 3. Independently produce actual 3D physics

The earlier mac-02 output already exists, so use a new path. The original
physics proof requires only NumPy and Pillow; no npm/esbuild is needed
when the optional offline bundling flag is omitted.

~~~bash
python3 -c 'import numpy, PIL; print("NumPy/Pillow OK")'
python3 -u scripts/run_rbc_visual_proof_package.py \
  --solver build/kyoto-final-cpu/ccp_rbc_coupled_tournament \
  --output output/rbc-geometry-proof-mac-03 \
  --frames 80 --geometry-seed 73013 --topology-seed 86011 \
  --subdivisions 8 --stride 3 --fps 12

open output/rbc-geometry-proof-mac-03/INTERACTIVE_ORIGINAL_PHYSICS.html

python3 -u scripts/rbc_benchmark_observatory.py \
  --research output/rbc-observatory-native-pilot-01/RBC_RESULTS.json \
  --proof output/rbc-geometry-proof-mac-03/NATIVE_FIVE_POLICY_SOLVER.csv \
  --output output/rbc-observatory-combined-01

open output/rbc-observatory-combined-01/RESEARCH_OBSERVATORY.html
~~~

The original mesh viewer now has selectable original material shading,
edge-length-change heatmaps and displacement-from-rest heatmaps, in
addition to orbit controls, a synced timeline, and per-policy CPU timings.
The colors derive from measured source vertices and do not change geometry.
The geometry HTML needs a network connection for pinned Three.js when
built without its optional offline bundling; the benchmark observatory
itself is fully offline.

## 4. Scientific honesty and larger visual scenarios

The 3D performance surface is a graph of actual speedup measurements,
NOT a simulated deformable mesh. The original cloth demonstration still
shows only a prepared folded source; adding analytic heatmaps does not
create new physically simulated materials. The existing six-material
AABB showcase is available through the older script
scripts/run_kyoto_visual_package.sh, but is not equivalent to a true
deformable multi-object coupled solver scene.

Substantial additional real solver/export scenarios should cover
multi-cloth self-contact, cloth stacks, deformable impacts, sphere packs,
ropes and springs, frictional contacts, and externally validated
reference motion. Each needs a native correctness oracle, fixed timing
protocol, source geometry and measured recordings before visualization.

Identical exact broad-phase candidates under identical solver physics
should produce identical trajectories. Better broad-phase speed is not
itself lower penetration, better folding, or calibrated physical fidelity.
Endpoint piercing audits and linearly sampled intermediate tests are not
continuous collision detection. Pilot timings provide no confidence intervals.
