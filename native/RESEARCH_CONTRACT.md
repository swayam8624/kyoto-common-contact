# Request-aware candidate service: implemented contract

The research hypothesis is that common conservative candidate infrastructure can serve different contact pipelines without absorbing their narrow phases or solvers. This prototype implements one bounded case: current-configuration cloth–sphere proximity queries inside a small C++ XPBD-style solver. A request selects margin and guarded reuse; it does not infer a solver or automatically choose an optimal algorithm.

## One geometric format

`Bounds{id, min, max, category, mask, movable}` contains a raw geometric axis-aligned box. IDs must be unique. Coordinates must be finite and ordered. An adapter supplies these boxes for its actual geometric primitives. The common service does not inspect vertices, topology, contact models or solver objects.

`QueryRequest{margin, displacementBudget, updatePolicy}` specifies a nonnegative finite proximity margin, nonnegative finite reuse envelope budget, and either `RebuildEveryIteration` or `GuardedReuse`. Half the margin is added to each primitive box in all coordinate directions. Eligibility requires both `(a.mask & b.category) != 0` and `(b.mask & a.category) != 0`. Pairs have canonical stable IDs, sorted and deduplicated. Touching boxes overlap. Sweep and prune and exhaustive reference use the same eligibility and overlap predicates.

The cloth adapter marks triangles as category 1 / mask 2 and the sphere as category 2 / mask 1. Thus this fixture requests triangle–sphere pairs only. The floor has a separate analytic response constraint; it is not a mesh broad-phase query. Other adapters must specify their own valid pair types and exclude adjacency as required by their methods.

## Conservative reuse certificate

On a guarded rebuild, the service stores padded boxes and expands movable boxes by the displacement budget. It computes and caches the actual pair list for those envelopes. Before every later query, it validates input and checks that every newly padded bound is contained in its corresponding stored envelope. The check also requires matching ID, category, mask, movable flag, request, backend and primitive count. If any check fails, it rebuilds before returning candidates. Reuse returns the cached list without calling the pair generator.

For each primitive i, let B_i be its current padded bound and E_i its stored envelope. If every B_i is contained in E_i, any overlap of B_i and B_j is also an overlap of E_i and E_j. With unchanged pair eligibility, the cached envelope-overlap list therefore includes every current padded-box overlap. Geometry within the requested Euclidean distance has overlapping half-margin-padded boxes, provided the raw geometric bounds are conservative. Extra pairs are allowed. This is a conditional enclosure argument, not an empirical guarantee for arbitrary adapters.

The budget controls how much geometric motion the existing certificate may tolerate. A larger budget can reduce rebuilds and increase false positives. It is not a permission to skip a containment check or tolerate missed contact. This implementation validates all bounds on every query, including reuse.

## Actual solver coupling

`ClothSimulation::step()` integrates the current particle state, runs eight XPBD distance-constraint iterations, and invokes the shared service for the resulting current geometry each iteration. Candidate triangle IDs drive exact sphere–triangle closest-point checks. The active precise set is collected before sequential collision projection, so changing the number of extra candidates does not reorder response. A final query supplies diagnostics and render classifications. The renderer consumes the same `positions()` array in the same executable; it has no mesh-file or animation playback path.

An exhaustive sphere–triangle proximity oracle checks coverage at each queried iteration and in final diagnostics. An omission pauses the interactive simulation. Disabling collision response is an explicit separate control; broad phase itself does not create contact forces. Cached candidates may include extra triangles compared with a freshly built current-bound query; the precise stage rejects them.

## Verification and evidence

`tests/live_cloth_tests.cpp` covers independent service input validation, masks, inclusivity, request/metadata/backend invalidation, cache reuse and escape, SAP/reference agreement, paired physical state equivalence, deterministic integration, response on/off and a 1000-step finite-state check. These tests do not constitute material calibration or production cloth validation.

`apps/live_benchmark.cpp` runs five paired trials with alternating call order, 300 steps per variant and 30 warm-up steps excluded per trial. It compares every particle coordinate and rejects any position error of at least 1e-9 or required-pair omission. The recorded run had maximum position error 0, zero omissions and 1350 measured steps per variant. CPU timing includes oracle diagnostics and excludes rendering. See the raw CSV and summary under `output/native`.

Native GPU captures at step 120 show baseline and guarded states with the same position checksum. The captured 120-step trace sequences also match checksums. These are supporting provenance; full coordinate comparisons in the paired benchmark provide the stronger equivalence check.

## Limits and next adapter

Queries cover current discrete solver configurations. They do not provide a swept enclosure over a timestep, CCD, triangle–triangle self contact or tunnelling prevention. The XPBD-style test solver is independently written and not an official library integration. We have not established IPC, OGC or CCFR guarantees, convergence or cross-solver speedups.

A future adapter must declare the method's motion interval, inflation and eligibility requirements, conservatively enclose all required primitives, then retain that method's exact narrow phase and solver. A swept-query extension requires a new explicit contract and tests; current bounds must not be silently interpreted as continuous motion bounds. Evaluate one codebase first against its original generator using matched settings, required-pair coverage, collision outcomes, stage cost, total runtime and the method's own convergence measures.
