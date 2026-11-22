# Scientific and cinematic visual atlas

Status: specification for future real solver-derived visuals. No item here is claimed complete. The target is a genuine scientific observatory and striking recorded scenes, not decorative animated cartoons.

## 3D views and cinematic evidence

| ID | Visual type | What viewers should see | Required trustworthy data |
|---|---|---|---|
| V01 | Cinematic hero scene | Free soft object, articulated tool, camera move, studio PBR shadows and readable materials | Real simulated vertex positions and solver parameters |
| V02 | Surface contour topographic map | Dense isolines draped on deforming mesh, scalar shading and exact range/colorbar | Actual measured displacement, strain or surface scalar |
| V03 | Cutaway volumetric iso-surfaces | Translucent shell, stress/displacement slices, nested contours and voxel/tet wireframes | True volumetric field; not valid on triangle-only surface |
| V04 | Rainbow particle phase portrait | Thousands of motion-traced particles with depth, speed-colored trajectories and contact splashes | Real particle solver output, velocity with units |
| V05 | Contact-event fireflies | Sparse bright dots at real vertex-face/edge-edge events and short-lived physically located trails | Independently verified CCD events with coordinates and time |
| V06 | Perfectly aligned before/after | Fixed camera, ghosted baseline and proposed trajectory plus deviation heatmap | Identical physics outputs; differences only if independently explainable |
| V07 | 3D swept bounding-volume forest | Motion tubes, refitted branches, tree cuts, invalidation lights, independent rigid meshes | Native runtime BVH and certified swept volumes |
| V08 | Embedded topological contact network | Mesh viewed as colored graph, edges appearing/disappearing, cliques, connected contact components | Mesh connectivity and real contact edge IDs |
| V09 | Separate interactive 3D graph | Force-directed node layout with physical anchor inset and linked event timeline | Contact graph data; layout is an abstraction, not mesh shape |
| V10 | Spacetime slices | Spatial bounds extruded over time, synchronized time scrub and onset of possible contacts | Valid motion envelopes and event timestamps |
| V11 | Dynamic local repair atlas | Newly dirty leaves, changed ancestors, whole-subtree rebuilds and costly fallbacks | Algorithm instrumentation, not artistic inference |
| V12 | Industrial/medical proxy scene | Flexible packaging or virtual instrument contacting deformable body in realistic scale | Real solver states; medical realism not claimed without calibration |
| V13 | Granular gravity scene | Hopper/avalanche/collapsing rocks with particles passing around machinery | Real DEM/particle adapter; separate research track |
| V14 | External truth overlay | Colored pointwise error between imported measured/reference motion and solver geometry | Registered ground truth, unit normalization and interpolation details |
| V15 | Contact density mural | Accumulated spatial hot spots and isolines evolving over 100+ frames | Verified event count per region, pressure only if forces known |
| V16 | Film-quality mechanism walkthrough | Macro shot inside the data structure switching to live deforming physical scene | Exact matching scene IDs and branch snapshots, no synthetic performance labels |

## Quantitative figures to accompany actual tables

| ID | Chart | Definition and interpretation |
|---|---|---|
| G01 | Paired speedup forest | Rows = scenes; x = baseline/SCA wall-time ratio; mark 1x, scene-bootstrap CI |
| G02 | Regime crossover heatmap | X = contact density, Y = normalized motion, cell = geometric-mean speedup, blank means unmeasured |
| G03 | Contour performance surface | X = primitive count, Y = local dirty fraction, Z = measured total latency, clearly show interpolation uncertainty |
| G04 | Timing decomposition | Actual milliseconds per step for bounds, refit, query, CCD, solver and allocations, stacked alongside raw table |
| G05 | Per-frame event and maintenance | Contact counts, candidate pairs, dirty nodes, rebuilds and end-to-end time on synchronized time axes |
| G06 | Graph topology evolution | Frame x contact component matrix, graph degree distribution, dynamic contact-edge births/deaths |
| G07 | Candidate amplification plot | Swept-AABB candidates / independently confirmed CCD events; no event? explicitly undefined, not infinity |
| G08 | Pareto efficiency frontier | Collision time vs peak memory vs precision, one dot per real method/scene |
| G09 | Tail latency distribution | Raw paired points and box/raincloud plot, P50/P95/P99 with enough independent scene samples |
| G10 | Ablation impact | Remove one mechanism at a time, show matched time delta and uncertainty; cannot sum dependent effects as independent |
| G11 | Failure and repair storyboard | Actual missed-pair or unsafe-bound counterexample, corrected run, step-by-step oracle table |
| G12 | Coverage matrix | Rows = solvers, columns = scene/material/scale/platform; status observed, failed or not tested |
| G13 | Scaling law plot | log N vs paid time and candidates with slopes and confidence, distinguish counts from milliseconds |
| G14 | Physical fidelity errors | Geometry intersections, substep validity and external alignment errors independent of speed comparison |

Every figure has a companion machine-readable table, method labels, seed, hardware, solver version, metric definition, uncertainty method and raw data path. The result should be beautiful because the physics and measurements are interesting, not because bad data are hidden.

## Deliverable definition

- Scene gallery: eight or more distinct *real* physically valid scenarios with free objects; minimum one self-contact-rich, one industrial-like and one rigid-deformable integration.
- Native Mac studio: SDL3 + Metal renderer, PBR, shadowing, pathline/contour layers, transparent diagnosis, timeline scrub and high-quality capture. Implementation pending.
- Quantitative lab: ParaView/VTK for contour/volume/streamline data; Python Plotly/Matplotlib for exact charts and interactively linked tables.
- Film export: Blender studio renders from the same immutable solver frames; consistent calibrated camera and units.
- Observatory: one timeline controls the physical replay, contact graph, hierarchy debug view, timing plots and event inspector.
- Output: 3 polished real-data films, full interactive gallery and 12-14 evidence figures as future goals, not yet produced.

Color semantics: sequential color maps for magnitude; diverging for signed error; categorical colors for algorithms and object IDs; rainbow palette only with a clear encoding purpose. All heatmaps have exact units, pinned limits, legends and error-state annotations. No fabricated pressure or stress field on a solver that never computes it.
