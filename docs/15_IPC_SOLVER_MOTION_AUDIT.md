# Pass 5B — Official IPC Toolkit audit of truly simulated 3D FEM motion

**Status:** Real external adapter implemented and connected to the pinned
IPC Toolkit v1.6.1 CI; run status must come from its original GitHub receipt.
No passing claim is made here before the full external job completes.

## The scientific question

A solver that enforces z>=0 at its vertices does NOT thereby become a
general mesh collision solver. Three different issues must be separated:

1. **One-sided plane admissibility:** every accepted moving vertex has
   z(t_end)>=0. Because the accepted trial path is piecewise linear
   between accepted solver-step states, this means all accepted
   *vertex* line segments stay within the positive-z halfspace if
   both endpoints are nonnegative. For this horizontal floor, it also
   contains every triangulated face as a convex combination of vertices.
2. **Upstream external CCD verification:** a triangulated patch of the
   same rigid floor can be checked by an independently compiled
   published vertex-face/edge-edge continuous detector.
3. **Self-contact robustness:** no vertex below an infinite plane
   does NOT imply the volumetric torus cannot intersect itself.
   Self-contact requires a different constraint/collision response
   strategy, so we explicitly audit for upstream possible self CCD
   signals without asserting that the current plane solver resolves
   them.

The external source program is
tests/ipc_solid_trajectory_audit.cpp.
It re-runs the actual C++23 900-step integrated hyperelastic body
(neo-Hookean stress, gravity, elasticity, rigid floor impulses) rather
than substituting an animated trajectory.

At selected accepted small timesteps it builds *actual* upstream
IPC Toolkit v1.6.1 LBVH on original connected surface vertices,
edges and faces plus a stationary finite triangulated floor patch.
The floor patch must cover the body's projected XY footprint;
it is geometrically different from the solver's infinite plane.

The upstream cross-object collision filter only considers
body-floor primitive combinations. Real official VF and EE candidate
stencils are evaluated through the real upstream continuous CCD
method on each accepted start/end step.

Independently, at a smaller sample of the same real timesteps the
upstream solver's actual surface self-contact features are audited
with same-object pairs permitted except for topology exclusions.
Detected self-events are disclosed, NOT silently reclassified
or reported as solved by the native plane-only reference solver.

## Sampling and limits

The physical solver integrates 900 time steps of size 0.00075 s.
The external geometric audit inspects accepted steps divisible by
20 plus a denser 5-step window near first contact (335–425), and
it performs additional self-contact surveys every 150 steps.

This is intentionally a *partial* audit. It does not prove
all 900 steps safe against general surface intersections, and it
does not produce an independent CCD algorithm.

Even an upstream candidate hit at t≈0 can be an existing
geometric contact or a conservative CCD event. The solver's
lumped plane impulse count is a different physical quantity.
The two should not be equated by number or time.

Per-step observed official LBVH build+query and narrow-phase CCD
wall times are recorded and include actual work in this audit.
But one GitHub session on unevenly selected steps is NOT a
randomized official-LBVH-vs-SCA timing comparison.

No SCA incremental broadphase is used in this experiment.
No speed advantage may be claimed.

## Trace receipt

The optional C++ executable exports:

- official_ipc_on_real_fem.csv:
  native step/time, accepted plane margin, native impulse count,
  official VF/EE cross-object broad-phase candidates,
  official VF/EE genuine CCD events, conservative time of impact,
  upstream paid milliseconds, and separately marked self audit events;
- manifest.json: native solver steps, sample count, explicit
  absence of general solver self-contact response.

scripts/render_ipc_solid_audit.py consumes ONLY the actual C++/IPC
audit trace. Five scientific charts show:

1. official cross-object candidates against actual CCD events;
2. physically solved plane impulses versus different IPC CCD events;
3. actual upstream paid building/querying and narrow CCD cost;
4. potential unresolved self-contact events (never omitted);
5. nonnegative plane margin on the selected real integration steps.

## Reproduction

    cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
      -DKYOTO_WITH_OFFICIAL_IPC=ON \
      -DIPC_TOOLKIT_WITH_SIMD=OFF
    cmake --build build-ipc \
      --target kyoto_official_ipc_solid_audit --parallel 4

    ./build-ipc/kyoto_official_ipc_solid_audit results/ipc-solid-audit

    python3 -m pip install numpy pandas matplotlib
    python3 scripts/render_ipc_solid_audit.py \
      --input results/ipc-solid-audit \
      --output results/ipc-solid-audit/visuals

The separate original upstream workflow is
.github/workflows/ipc-reference.yml.

## Next indispensable scientific gates

A full safe-mesh-contact physical solver still needs:
- CCD checked at every genuine accepted integrator step for ALL
  relevant feature interactions, not a selected subset.
- A robust constraint/barrier strategy for self-contact, moving
  nonplanar objects, and rigid-deformable interaction.
- Independently checked physical energy/penetration/CCD event truth.
- A second solver and heldout real-world geometry cases.
- A strict paid comparison of our new SCA *incremental maintained
  frontier* against actual current official LBVH across all
  collision-search and solver costs.

This audit improves *measurement of the solver's geometric limitations*.
It does not convert a simple plane-response solver into a general IPC
deformable collision solver by assertion.
