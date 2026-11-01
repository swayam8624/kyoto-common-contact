# November 2, 2026 — Kyoto presentation readiness

## Which experiment is actually the research prototype?

**Primary:** `./open_3d_demo.sh` launches a live C++23 cloth simulation with
XPBD-style distance constraints, a fixed sphere, a conservative shared
sphere/triangle broad phase, precise sphere–triangle distance/contact response,
and guarded candidate reuse. It is not IPC, OGC, or CCFR.

**Supporting:** `./run_demo.sh` launches a separate Node/browser 2.5D
projection of a C++ rigid-sphere core. Its independent graph-island and
analytic sphere TOI demonstrations are conceptually useful but **do not** use
the cloth shared service and are not integrations of real XPBD/OGC methods.

**Offline fallback:** `output/Common_Broad_Phase_Demo.html` is a standalone
point/triangle candidate illustration; `output/research3d/Common_Broad_Phase_3D.mp4`
is a prerecorded Blender-generated cloth experiment with C++ query analysis
of exported mesh snapshots. Neither is live native cloth physics.

## Reliable preparation

From the checkout, run:

```sh
./scripts/prepare_meeting.sh
./open_3d_demo.sh
```

Review `docs/MEETING_RESULTS.md` and
`output/native/benchmark-postfix.svg` **after** the preparation script
succeeds. Do not reuse the pre-fix 2.351/0.999 ms values as post-fix timings.

Do not claim a new speedup until the fresh CSV is available.

## Recommended 4-minute live explanation

1. **Live geometry:** start native cloth, stage 1. Point out the 625
   particles, 1,152 triangles, sphere, eight XPBD-style constraint iterations.
2. **Contract:** stage 2. Explain geometric bounds, stable primitive IDs,
   category/mask eligibility, half-margin padding and certified envelopes.
3. **Broad to narrow:** stages 3 and 4. Candidates are conservative,
   amber; actual nearby triangles are green. Explain extra candidates.
4. **Guarded cache:** pause (Space), step (N), toggle rebuild/reuse (Q).
   Certified enclosure reuses a pair superset only if every current padded
   bound remains inside its cached envelope. Toggle M to show invalidation.
5. **Correctness:** press V for independent oracle ON; demonstrate zero
   observed misses, explicitly scoped to this discrete sphere/triangle query.
6. **Benchmark:** insert fresh `benchmark-postfix.svg` into Gamma slides,
   report measured counts and the oracle/diagnostics cost boundaries.
7. **Islands/CCD extension:** optionally show separate browser demo.
   Clarify these algorithms live in the older rigid-sphere testbed only.

## What not to claim

- Not an official IPC, OGC, CCFR, or PositionBasedDynamics integration.
- Not cloth self collision, continuous mesh CCD, or guaranteed nonpenetration.
- Not method-independent contact-island independence. Cloth internal
  constraints couple particles even without direct sphere contact.
- No cross-solver performance benefit is established.
- The final global triangle scan remains for viewer diagnostics even when
  the per-iteration oracle is disabled.

## Core scientific theorem of the current fixture

If each current padded primitive bound `B_i(t)` is contained in the cached
envelope `E_i`, and pair eligibility and query semantics remain unchanged,
then current padded AABB overlaps are a subset of cached envelope overlaps.
Thus cached candidates conservatively cover the current discrete broad-phase
query. This is **not** a continuous-time collision guarantee.

## Changes for the meeting

The repair branch changes the normal solver to examine only candidate triangles,
makes the exhaustive oracle independent and explicitly toggleable, adds
precise/oracle/display diagnostic costs and counters, and runs an oracle-checked
preflight outside timed benchmark runs. These statements still require a passing
fresh build and test log before being presented as verified.

## Dynamic contact-island scene (rigid-sphere supporting demo)

The legacy browser scene now begins with two spatially separated moving
sphere groups whose paths intersect in the same Z plane. It forms actual
cross-group narrow-phase contacts rather than permanently separated static
clusters. The graph is reconstructed from verified contacts each frame, and
colors update from connected components. To demonstrate:

```sh
./scripts/launch_meeting.sh islands
```

Select **02 · Contact islands**. Observe the two groups approaching,
pause around their interaction, then turn contact/island overlays on/off.
This graph is for independent rigid spheres with an impulse solver, *not*
the internal constraint graph of the cloth/XPBD experiment.
