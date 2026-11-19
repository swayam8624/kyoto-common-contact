# Kyoto RBC — what the ugly intersecting-cloth GIF exposed and what actually changed

**19 November 2026. Real solver postmortem, not a promotional graphics claim.**
Research branch: research/reach-budget-calculus.
Frozen historical V4 and pre-cleanup annotated archive remain untouched.

## 1. The exact failure of the former experiment

The original 'prefold_relax' initialization reflected the lower
half of the first real cloth sheet directly into the upper half,
adding only a tiny 0.022-unit vertical offset. This created a
geometrically INTERSECTING input. The existing discrete XPBD-style
contact solver could then push some vertices away but did not
actually disentangle the cloth. It limited each vertex's total
collision correction to 0.03 simulation units per iteration,
with only five structural/contact iterations per frame.

Crucially, exact broad-phase pair parity across all five policies
was never a proof of correct cloth dynamics. It only established
that all five candidate generators sent identical input to the
SAME imperfect narrow-phase/contact projection.

The earlier impressive figures (121,376 real same-sheet contact
corrections and 39,245 self-crossing detections, three source
cases) therefore exposed an invalid contact-rich stress fixture,
not realistic penetration-free cloth. Those numbers are truthful
counts of attempted projections and observed edge-face
crossings, NOT proof that collisions were resolved.

I independently reopened the original native 3D vertex and
triangle JSONL from the previously published GitHub evidence
(artifact from run 44551401172). Using a SEPARATE NumPy
strict-interior segment/triangle piercing test on the final
solver state (not the live broad-phase, not the renderer),
the first original source still had:

| Physical frame | Nonlocal same-sheet triangle pairs STILL intersecting after step |
|---|---:|
| 0 | 45 |
| 10 | 34 |
| 40 | 33 |
| 79 | 31 |

This is the user's exact complaint: the geometry was genuinely
intersecting. The original crude 2D painter/wireframe renderer
also produced inaccurate depth ordering and visually obscured
the sheet fold. Both failings were real and needed separate fixes.

**Original invalid scenario remains available unchanged**
as prefold_relax, a protected negative control.
Do NOT erase its failure and do NOT reuse its 1.08032x
whole-step ratio as evidence of high-quality physical simulation.

## 2. The physically different input: rounded_fold_relax

The new option uses a SMOOTH finite-radius U-turn, not a
self-penetrating mirror. The first 13x13 cloth sheet remains a
real deformable mesh with its original constraints, topology,
pinned anchors and solver; its lower flap bends around a
circular hinge rather than being teleported through itself.

Let s measure distance from the hinge into the lower flap,
R = 0.11 simulation units and theta = min(s/R, pi).

The rounded sheet has local offsets:

    y(s) = -R sin(theta) + max(0, s - pi*R)
    z(s) =  R (1 - cos(theta))

The outgoing lower flap becomes a parallel upper layer with
nominal clearance 2R=0.22. This is a PREPARED state, not a
simulation of a spontaneous fold. The original second cloth
sheet is rigidly translated by +3.65 units in X into a separate
comparison bay, including its reference anchors, so it
cannot visually/physically occlude the first folded sheet.
No original spring rest length or triangle connectivity is
changed. The previous unmodified simulator fixtures still
run for historical comparison.

This entirely new fixture is implemented only in the RBC
research branch, with new explicit entry points:
MultiCloth::prepareRoundedNonintersectingFoldForResearch()
and ccp_rbc_coupled_tournament scenario rounded_fold_relax.

## 3. Fail-closed geometric validity, not just an AABB hash

New C++ source geometry oracle:
MultiCloth::countNonlocalSelfIntersections().

It enumerates all nonlocal same-sheet triangle pairs,
uses independent exact current AABB culling, and counts
STRICT-interior current edge/triangle piercing. It
does not depend on a broad-phase policy or shared
collision candidates, and its cost occurs AFTER measured
solver.step() so performance accounting remains honest.

The test suite preserves the original overlapping initial
geometry as a required negative control, and checks the
new source for zero strict geometric piercing across
the rebuilt SAP, Guard, frozen V4, RBC indexed and RBC
flat real solver arms. ANY remaining observed strict
piercing causes a failing CI run, rather than a success GIF.

The Python independent geometry auditor operates from
the original source JSONL bytes, validates full per-frame
SAP particle-state equality, and recomputes the original
C++ endpoint piercing count independently in NumPy.

It also checks SEVEN intermediate states at 1/8 to 7/8
of the LINEAR interpolant between each pair of actual
simulation frames. This is a stronger diagnostic for
tunneling than endpoint-only evaluation, but it is
NOT full continuous collision detection.

Original GitHub Actions source:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44557971724

Results across 3 independently perturbed geometry/topology
sources, five native algorithms, 80 physical frames each:

| Independent geometry gate | Observations | Nonlocal strict piercings |
|---|---:|---:|
| Actual source initial state | 3 | 0 |
| Actual source end-of-frame triangles | 240 | 0 |
| Actual linearly interpolated subframe states | 1,680 | 0 |
| Source physics comparisons to SAP | 1,200 policy/frames | same positions within 1e-7 |

The newer source also retains TRUE same-sheet contact responses,
rather than trivially making every cloth collision impossible:

| Source geometry seed | SAP actual self-contact corrections | Actual maximum strain-like edge stretch ratio |
|---|---:|---:|
| 73013 | 215 | about 1.270 |
| 74017 | 141 | about 1.298 |
| 75029 | 288 | about 1.304 |
| Total | 644 | values above are condition maxima |

The present solver still has contact corrections and significant
deformation; this audit simply establishes that strict nonlocal
triangle piercings were not detected at the sampled times.
It does NOT prove an unconditionally intersection-free
solver or accurate cloth continuum behavior.

## 4. Actually improved graphics, no fabricated cloth

The first preview was an unlit, heavily edged
orthographic triangle painter. It had inadequate
surface occlusion and confusing overlapping views.

NEW script scripts/render_rbc_physics_studio.py:
- reads the actual five policy JSONL source meshes;
- fails unless all five original trajectories have the same
  vertex positions, source topology and native contact counters;
- uses a numerical per-pixel software Z-buffer for true
  front/back surface visibility and sphere occlusion;
- shades the original cloth triangles with a fixed camera,
  separate true two-sheet palettes and actual measured CPU
  times, avoiding chaotic triangulation outlines;
- clearly marks the INDEPENDENT geometric endpoint validity
  with green status or a red penetration warning;
- exports a looping 3D GIF, PNG preview and SHA-256 source
  manifest. No original physics vertex is changed.

NEW interactive artifact:
web/rbc_verified_physics_viewer_template.html plus
scripts/build_rbc_physics_viewer.py.

It gives five synchronized true 3D Three.js source-backed
panels, original physically measured contact and step time
readouts, a frame scrubber, synchronized camera orbit,
wireframe toggle, optional sphere visibility, second-sheet
isolation and source SHA receipt. All five native trajectories
are EMBEDDED in a portable HTML. With --offline and pinned
Three.js 0.180.0 + esbuild 0.25.12, it requires no remote
resources, CDN or HTTP server. Graphics-only WebGL-disabled
browsers still preserve all source timing/contact/geometry
readouts and show a clear fallback instead of crashing.
A dedicated original source-backed GIF remains available.

The shader/color/fabric shading is PRESENTATION. It does
not constitute a novel simulator improvement.
Original topology and true physics never change for a
different candidate generator.

## 5. What the new narrow physical cohort says about RBC timing

On three rounded-fold 80-frame one-shot Linux physics
scenes, exploratory whole-step Guard/RBC-flat ratios,
after the first eight frames, were about:

- Seed 73013/86011: 1.2532x
- Seed 74017/86019: 1.2242x
- Seed 75029/86029: 1.2043x

Equal-scene geomean: about 1.2271x.

**DO NOT call this statistically proven 22.7% general speedup.**
Three closely related prepared fixtures and one Linux
timing session per source have no independent A/A noise
floor or population confidence bounds. The original
54-scene query-only material cohort still showed the
flat RBC at a losing 0.87099x Guard/flat geomean, with
severe cushion negative cases. Frozen V4 remains a strong
standalone CPU candidate method. The whole-step metric
cannot be generalized to calibrated FEM/IPC without
external solver tests.

## 6. One reproducible proof-package command

Prerequisites: C++23 build, Pillow, NumPy and
(optionally for OFFLINE HTML) pinned local Node packages.
Do NOT run historical scripts into an existing output dir.

    cd ~/Desktop/Programming/kyoto-common-contact
    git switch research/reach-budget-calculus
    git pull --ff-only origin research/reach-budget-calculus
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_rbc_coupled_tournament ccp_rbc_physical_geometry_tests
    ctest --test-dir build/kyoto-final-cpu -R 'rbc_' --output-on-failure
    python3 -m pip install numpy Pillow
    npm install --no-save --no-package-lock three@0.180.0 esbuild@0.25.12
    python3 scripts/run_rbc_visual_proof_package.py --solver build/kyoto-final-cpu/ccp_rbc_coupled_tournament --output output/rbc-physical-visual-proof-local --frames 80 --geometry-seed 73013 --topology-seed 86011 --subdivisions 8 --stride 3 --fps 12 --offline
    open output/rbc-physical-visual-proof-local/INTERACTIVE_ORIGINAL_PHYSICS.html
    open output/rbc-physical-visual-proof-local/ACTUAL_CLOTH_FIVE_POLICIES.gif

The integrated command refuses overwriting existing research
receipts; use a unique new output directory for reruns.
It packages true C++ source records, 5 independent JSONL,
an independent geometry audit, the 3D GIF/PNG, the offline
orbit viewer and a complete original SHA-256 manifest.
The historical source archives are NOT overwritten.

## 7. Research limitations that still must be addressed

- Strict interior edge/face intersections do not cover ALL
  degenerate or coplanar triangle overlap configurations.
- Seven sampled LINEAR subframes are not exact continuous CCD
  over the actual XPBD five-iteration within-step trajectories.
- True collision quality needs independent external
  penetration-depth, CCD and energy/strain/constraint audits.
- A partly artificially pre-folded mesh is not a verified
  natural fold from originally nonoverlapping flat geometry.
- Broad-phase performance needs new independently held-out
  physical scenes, repeated A/A controls, confidence intervals,
  optimized Guard and IPC/CCD external library benchmarks.
- Correct broad phases MUST return identical physics;
  better visual folding would require a separate physical
  solver advancement, not a fake method-specific render.

**Research principle:** Exact broad-phase candidate parity
is necessary, not sufficient. Valid physics needs a
second independent geometric and temporal correctness gate.
Negative source trajectories and failed visuals are
evidence to preserve, not to hide.
