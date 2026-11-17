# Kyoto Collision Broad Phase — Final Validated CPU Release

**Status: FROZEN ENGINEERING DELIVERABLE. No further parameter tuning, new
method families, repeated benchmarking, or training are part of this release.**

This is the final **validated CPU AABB-candidate** deliverable based on all
completed experiments, including the adverse results from V5 and V6. It is
not claimed to be a novel optimal broad-phase algorithm, a Metal GPU
implementation, an exact continuous-time collision detector, or a fully
integrated replacement for the coupled deformable solver.

## Production candidate selected

**Name:** `guard_sap_bounded_trial` — single-backend-per-frame Guard/SAP
controller, developed and benchmarked in V4.

**Reusable public API:** `include/ccp/final_broadphase.hpp`:

- `ccp::final_broadphase::GuardSAPBoundedTrial`
- `ccp::final_broadphase::PersistentSAP`

The implementation was **extracted without modifying the thresholds or
switching logic** from the measured, repeatedly verified V4 algorithm.

**What happens each frame:** Guarded pair-cache reuse is used by default.
Repeated *measured costly Guard rebuilds* may trigger a bounded SAP trial.
The controller observes actual query cost, rejects unpromising SAP trials,
and returns to Guard on sustained quiet motion or observed SAP deterioration.
**Only one underlying backend executes per frame.** Costly first visits
and switches are included in measured timings.

**Guarantee:** For finite valid AABBs with stable ordered IDs and the
supplied category/mask rules, output is a sorted unique set of all **current
inclusive-AABB overlapping candidate pairs**. Geometry, never ML, determines
which pairs exist. The controller is stateful: instantiate once per object
stream and do not share concurrently across worker threads.

## What is actually proven by the supplied experiments

The user ran complete, predeclared CPU identical-snapshot tournaments with
three randomized method-order repetitions per scene. The reported tests
verified pair-set hashes/counts against exact SAP on every frame and a
sampled independent exhaustive reference on smaller scenes.

On the V5 864-cell benchmark (seed IDs 3203,3217,3251,3299,3313,3343,3371,3407):

- V4 Guard/bounded geometric-mean scene speedup: **1.091216×**.
- **667/864** scenes faster by mean query time.
- Summed scene-mean Guard/bounded speedup: **1.328684×**.
- Guard A/A speed ratio: **1.000459×**.

On V6's **different** 864-cell independent confirmation (seed IDs
5101,5113,5147,5153,5171,5197,5231,5261):

- V4 Guard/bounded geometric-mean scene speedup: **1.083201×**.
- **596/864** scenes faster by mean query time.
- Summed scene-mean speedup: **1.316721×**.
- Guard A/A speed ratio: **0.999626×**.

**Neither result is an end-to-end solver or GPU speedup.** The 864-cell
comparisons cover related synthetic scene generators on one machine. The
results include losses on particular workloads; winning on every scene
was never demonstrated. The newer V6 unified certificate hierarchy
achieved only **0.629685×** geometric speedup versus Guard and must **not**
be represented as an improvement.

Source evidence: the user's local paths
`~/Desktop/Programming/kyoto-exact-rumble/output/kyoto-rumble-v5-full`
and
`~/Desktop/Programming/kyoto-unified-v6/output/kyoto-v6-independent-confirmation`,
and the user's uploaded `RUMBLE_PATTERNS.json` files. Full raw result
directories remain local and are NOT copied into this release branch.
The authoritative experimental histories, including unsuccessful models,
remain on the `research/kyoto-exact-event-field-rumble` and
`research/kyoto-v6-certified-interaction-hierarchy` branches.

## Minimal integration

```cpp
#include "ccp/final_broadphase.hpp"

ccp::final_broadphase::GuardSAPBoundedTrial controller;

// Retain this controller for successive snapshots of one fixed-ID scene.
// Each snapshot is std::vector<ccp::live::Bounds>, with stable ordered IDs.
const auto result = controller.query(snapshot);

// result.pairs: exact, sorted, unique current overlapping AABB pairs.
// result.action: "global_guarded_exact" or "sap_persistent_coherent".
// result.switched: whether the selected backend changed.
// result.trialFrame: whether this frame is within the bounded SAP trial.
```

When integrated into the existing solver, call once for each requested
candidate set and pass `result.pairs` through the *existing* precise
contact stage and topology filters. Do not replace continuous-time tests
or assume these broad-phase pairs guarantee self-intersection prevention.
The measured V4 implementation was originally in
`apps/material_replay_benchmark.cpp`; the exact same policy now comes
from the reusable header so downstream clients do not import an app.

## Verification without another deathmatch

The release uses a **short, blocking GitHub Actions gate** in
`.github/workflows/kyoto-final-release.yml`: compile the reusable API
and existing material exporter/replay, run the public API 90-frame/512-box
exact brute-force test, and run the existing learned/SAP/Guard full replay
oracle CTest. No new 864-scene workloads, training jobs, Metal runs or
multi-hour scripts. The release gate's success is the condition for
calling this source build verified; the prior experimental version was
already verified independently.

**Optional, not required for accepting the results:** build the small
test targets and run their two CTests; never re-run the exhaustive scripts
simply to use the final release.

## Explicitly excluded from this final result

- **Original MLP:** retained as a research baseline; not the champion.
- **V5 standalone BVHs and fixed/signal patches:** promising only for
  particular workloads, not measured as an inexpensive deployable online
  strategy across all scenes.
- **V6 Unified Certified Interaction Hierarchy:** prototype failed overall;
  not merged into this release.
- **Kinetic event deadlines and wave-based prediction:** research ideas, not
  solver-certified performance results.
- **Universal novelty/SOTA claims:** not supported by completed experiments.
- **Coupled simulator integration and measured physics-step acceleration:**
  original coupled simulator remains; this final selector is an independent
  reusable CPU AABB module and still requires dedicated integration if it
  is to replace the original solver's online controller.

## Final engineering decision

**SHIP / FREEZE V4 as the validated reusable CPU broad-phase selector.**
Keep all unsuccessful and alternative research methods separately archived.
Don't change thresholds, don't retrain the MLP, and don't launch another
exhaustive tournament to justify this release. Future scientific claims
would be a new, separately proposed project—not a prerequisite for this
finished engineering deliverable.
