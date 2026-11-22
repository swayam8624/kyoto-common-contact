# Development and validation plan: native evidence before cinematic claims

## Gate A0 — clean correctness foundation (implemented in this branch)

C++23 header defines primitive endpoint swept AABBs, stable IDs, finite-box validation, inclusive candidate overlap, bidirectional category masks and a fixed SAP baseline. An independent quadratic oracle checks parity. A fixed-seed corpus includes degenerate boxes, touching, high motion, masks and invalid inputs.

Limit: Stage 0 proves nothing about true triangle CCD, real contact, solver timing, GPU performance or novel algorithm.

## Gate A1 — independent standard implementation

Build a pinned official IPC Toolkit adapter for the same triangle primitives, thickness and motion model. Compare against its official current LBVH, spatial hash where available, a fixed SAP and the exhaustive reference on feasible sizes. Capture exact original primitive IDs and continuously computed event sets. A proxy reimplementation does NOT count as IPC.

Require edge-edge, vertex-face, self-contact, adjacency, thickness, near-degenerate, rapid motion, and nonlinear path-envelope cases. Include ASan, UBSan, adversarial fuzzing, IEEE boundary tests and cross-platform native CI. External data version and licence are recorded.

## Gate A2 — genuine solver/data integrations

Primary: IPC-compatible deformable-contact solver on replayable, physically valid moving triangle mesh scenes. Secondary: a separate real solver, with explicit adapter contract, not two wrappers around the same engine.

Scene portfolio (distinct geometries, not one cloth with differently colored balls):

S01: a fully free deforming thin membrane twisted into itself.
S02: a soft robotic gripper compressing a deformable shaped part.
S03: a flexible net dynamically catching and wrapping an object.
S04: a deforming ring/tube passing through articulated rigid obstacles.
S05: a layered packaging seal undergoing repeated machine compression.
S06: a medical-like tube contacted by a simulated instrument, clearly a geometry proxy absent calibrated material evidence.
S07: a dense multi-object flexible stack with sharp, repeated regime changes.
S08: a folded triangulated shell with intense same-object self-contact.
S09: a held-out asset-derived industrial geometry sequence with licence and provenance.
S10: an adversarial randomized suite emphasizing topology/mask changes and huge movement.

Granular avalanche, falling mixed spheres, rockfall and particle trails are a SEPARATE particle-adapter track; do not substitute them for triangle-mesh proof. Fluids and fracture are not original-scope claims. Scenic real-world mesh assets are valuable only if the solver genuinely drives their deformation.

Per scene require actual source mesh, physically meaningful parameters, accurate motion integration, contact event logs, independent geometry quality audits and full reproducibility.

## Gate B — baseline truth and an actual new algorithm

First measure fixed official LBVH and SAP in the full solver. Run profile traces, find the specific bottleneck and construct one SCA method answering it. No new selector/ML system without a profiler-identified reason. Implement versioned dirty propagation, local refit and subtree rebuild, full fallback, and parity tests. Pre-register the cost model, update rule, and primary claim before held-out measurement.

Every experiment captures all paid work: input conversion, boxes, motion bounds, dirty tracking, index construction/refit, filtering, pair normalization, CCD/narrow phase, constraint assembly, solver and memory allocations. Separate CPU, Apple Metal GPU and cross-transfer costs.

## Gate C — controlled experiments

Rows: scene x mesh scale x motion speed x contact density x material x seed x method x platform. Use at least five independent seeds for planned regimes, randomized/counterbalanced execution order, repeated A/A duplicate, thermal/power state, identical caches and warmup policy, failures and timeouts. A repeated timing on one seed is NOT a new independent scene.

Methods: exhaustive on feasible sizes, fixed SAP, latest pinned official IPC Toolkit LBVH, SCA, plus a baseline A/A duplicate. Bullet DBVT is optional extra rather than sufficient published comparison.

Publish paired per-scene ratios, bootstrap scene-level confidence intervals on log ratios, P50/P95/P99, worst cases, energy/memory, candidate amplification versus independent CCD events, contact quality, exactness and partial-cost breakdown. Validate geometry equivalence when the same narrow-phase events are fed the same solver.

Analyze Amdahl's law: reducing an insignificant broad-phase component cannot make the whole solver dramatically faster.

## Gate D — untouched challenge and independent review

Freeze code, scene split and primary statistical endpoints. Test a never-used confirmation cohort, ideally on a second CPU/OS and different solver. Independently inspect source hashes, negative cases, per-scene traces, oracle agreement, sampling uncertainty, baseline version, license and actual screenshots.

Require a completed prior-art matrix; dynamic BVH refitting, kinetic front tracking, region clustering and cost-based maintenance are not themselves new.

## Gate E — visuals and paper only after evidence

An observatory reads immutable generated traces, plus true solver states, and offers measured real-time, contour, topology, event, scaling and statistical views. It cannot invent scalar fields, contact events, or physics. Export scientific VTU/VTP/VTK plus camera-stable mesh replays for Blender/Metal, with data checksums. Research output is separate from visual polish.

## Parallel work streams with review owners

Geometry/invariants; algorithm/profiling; solver/scenes; renderer/scientific visualization; independent reproducibility and skeptical review. All five report to one acceptance matrix. No visual deliverable is allowed to self-certify physical correctness. No speed-up script is allowed to self-certify benchmark fairness.

## Failure conditions that stop a claim

Missing a true collision, unbounded wrong pruning, stale structural state, missing original receipts, mismatch from oracle or reference, solver-state divergence, material unrealisticness, benchmark setup drift, performance gain disappearing on official LBVH or unaddressed novelty overlap. A negative result remains in the archived experiment table.
