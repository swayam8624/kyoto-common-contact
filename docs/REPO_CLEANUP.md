# Kyoto repository cleanup — executed and traceable

**Clean branch:** \`feature/kyoto-benchmark-visual-cleanup\`
**Original preserved branch:** \`release/kyoto-broadphase-final-validated\`.
**Original cleanup commit:** \`24c8c640f713af8da1c006168b4e208a045f2d51\`.

## Actual destructive changes made (new branch ONLY)

Before cleanup, the final-release tree contained **4,522 tracked files**
and **258.6 MB** of checked-in file contents. The first cleanup commit
removed **4,308 tracked files, approximately 239.5 MB** and left **214
tracked files, approximately 19.2 MB** before the new benchmark/visual
files were added.

The cleanup removes or untracks these **generated or duplicated artifacts**
from the new branch (none of the historical Git objects are rewritten):

* \`.presentation-rebuild/\` — 4,104 files, including an entire vendored
  macOS Python environment, compiled wheels, generated PPTX/render images.
  Largest single category, ~107.5 MB.
* \`.presentation-native/\`, \`.presentation-3d/\`,
  \`.presentation-refine-2026-11-02/\`,
  \`.presentation-research/\`, \`.presentation-review-2026-11-02/\`,
  \`.render3d/\` — disposable deck/graphic build intermediates.
  Final presentation retained at
  \`output/Online_Common_Broad_Phase_Research_Layout_Final.pptx\`.
* \`output/native/native-{initial,baseline,adaptive,live}.ppm\` —
  four redundant uncompressed ~16 MB framebuffers. Existing PNG
  versions retained for visual historic evidence.
* \`output/research3d/video_frames/\` — 30 generated PNG frames,
  while source Blender file, existing MP4 and manifest remain.
* Older duplicated dated PPTX builds in the root of \`output/\`,
  plus one temporary \`.blend1\` backup. Final deck retained.

**Not deleted:** solver source, native viewer, final V4 selector,
material fixture/replay, official-style Bullet DBVT integration, model
training code and regression tests, historical November benchmark
evidence under \`output/review-2026-11-02/\`, README/runbook material.
The complete old presentation directories are still available from
the preserved final-release Git branch, not copied into the cleaned
current tree.

**Git history is not rewritten:** a fresh normal clone may still
download the old large objects from earlier commits; slimming history
requires a separate disruptive rewrite and coordination with every
existing clone. This cleanup intentionally does not break commit IDs,
branches or historical evidence.

## Source-level reachability audit

The original tree has 15 C++ app source files, four
\`src/\` translation units, active
\`native/\` sources, the viewer, and tests. CMake explicitly
references the app translation units as runnable targets; simple
'grep found no caller' deletion would falsely classify several apps
because they are entrypoints invoked from shell scripts and CI.

The actual shipping CPU algorithm is now isolated in
\`include/ccp/final_broadphase.hpp\`, with the same measured V4 decision
logic. The compiler does **not** pay for older training scripts,
graphics presentations or V5/V6 headers when building this API.

\`scripts/audit_repository.py\` performs a reproducible read-only
source/CMake reference scan, reports status in JSON and Markdown,
and flags accidental recommits of generated assets. It deliberately
uses the phrase 'manually review', not an unverified claim that a
source file is never executed.

### Keep in the active workflow

| Source family | Reason |
|---|---|
| \`include/ccp/final_broadphase.hpp\` | Validated single-backend Guard/SAP CPU controller |
| \`src/live_cloth.cpp\` | Exact candidate/query implementation and solver baseline |
| \`src/research_multicloth.cpp\` / \`src/online_partitioner.cpp\` | Actual coupled cloth physics and original research controller |
| \`apps/material_replay_benchmark.cpp\` / \`apps/material_snapshot_export.cpp\` | Freeze-and-replay exactness and benchmark data contract |
| \`apps/visual_multicloth_export.cpp\` | Verifiable real mesh trajectories/quality diagnostics |
| \`tests/\` | Exact pair, solver and input-contract regression |
| \`benchmarks/\` | External fixture import and physics-quality evidence |
| \`scripts/run_kyoto_visual_package.sh\` | One canonical visualization entrypoint |
| \`scripts/render_kyoto_visuals.py\` | Exact AABB + real mesh looping renderer |

### Historical but still referenced — preserved, not silently deleted

The root's older \`ccp_service\`, geometry demos,
\`scaling_cpu\` / \`scaling_regional\` / Metal tools, CPU stress runners,
online trace, patch-sweep and ML training/ablation code have historical
CMake/CI or documentation roles. They do not enter final-V4 standalone
runtime unless explicitly built/called. Remove them from the main active
build **by selecting only the final preset** rather than breaking
recorded experiments and historical workflows.

\`\`\`bash
cmake --preset kyoto-final-cpu
cmake --build --preset kyoto-final-cpu
ctest --preset kyoto-final-cpu
\`\`\`

\`cmake --build --preset kyoto-visual\` builds only the visual and
material replay tools. The old research targets remain explicitly
buildable if somebody needs an archival replication.

### Performance pitfalls identified, NOT cosmetically 'optimized'

* Repeated current-bounds copying and full pair sort/materialization
  can cost measurable time in older generalized online/region paths.
* The V6 failed unified prototype's repair path performed
  \`O(dirtyCount * N)\` scans even when a hierarchy existed;
  that **failed experiment is absent from the shipping V4 code**.
* The old trained 12-feature MLP often chose Guard while paying
  feature extraction/inference overhead; ML is not in the final
  selected V4 policy.
* Shader/renderer snapshots, Python dependencies, PNG slides and
  one-off presentation builders are neither linked to nor executed
  by the final broad-phase runtime.

No hot-path transformation is being called a speedup without a new
paired benchmark. Rewriting the already validated V4 source for
stylistic reasons would invalidate its measured evidence and is
intentionally out of scope.

## Build hygiene and regression

* \`.gitignore\` prevents new dependency environments, KMBC binary
  snapshots, PPM captures, loops and work products from being
  recommitted to \`output/\`.
* \`CMakePresets.json\` provides named, target-limited
  \`kyoto-final-cpu\` and \`kyoto-visual\` build profiles.
* \`.github/workflows/kyoto-visual-cleanup.yml\` builds the actual
  C++ mesh exporter, runs the public V4 and learned native exact-pair
  tests, renders real GIFs, checks external OBJ import and strict
  quality metrics, then runs the 36-scene full one-command showcase.
* \`python3 scripts/audit_repository.py --fail-on-generated\` is
  a CI policy gate against reintroducing generated clutter.
* Historical full/deathmatch scripts still exist for research
  reproducibility but are not called in the final visual workflow.

## Cleanup decision

The main source is leaner, final release is clear, generated
dependencies are absent, and all original research evidence is
preserved by branch. **Don't erase experiment source merely because
it lost a timing competition**: that would destroy the baselines
needed to substantiate the final result.

No further multi-hour search or retraining is necessary.
