# Multi-cloth research benchmark — generated from measured C++ data

**Scope:** 1/2/3 continuously driven cloth sheets, two moving spheres, one custom XPBD-style solver, inter-sheet vertex–triangle and edge–edge discrete contact correction.
Every policy's particle states were compared against full SAP, with max allowed error below 1e-7.
These are not official IPC/OGC/CCFR simulation results and are not collision-free or CCD guarantees.

| Sheets | Policy | Mean broad (ms) | Mean precise + response (ms) | Mean complete step (ms) | AABB candidates | Cross-sheet corrections | Rebuilt region blocks | Reused region blocks | Skipped region pairs | Guard pairs rejected | Online splits | Online merges | Controller cost (ms) | Max state error |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | sap_rebuild | 0.659 | 0.102 | 0.913 | 2289.6 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | global_guarded | 0.304 | 0.123 | 0.547 | 3143.4 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | region_fixed | 0.466 | 0.085 | 0.696 | 2289.6 | 0.0 | 111.7 | 743.3 | 0.0 | 4304.4 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | region_adaptive | 0.448 | 0.086 | 0.680 | 2289.6 | 0.0 | 86.7 | 768.3 | 0.0 | 5707.4 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | region_sweep_fixed | 0.475 | 0.085 | 0.710 | 2289.6 | 0.0 | 54.1 | 359.4 | 88.3 | 4304.4 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | region_sweep_adaptive | 0.451 | 0.086 | 0.681 | 2289.6 | 0.0 | 46.5 | 360.9 | 89.5 | 4213.9 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 1 | online_contact_work | 0.493 | 0.093 | 0.732 | 2289.6 | 0.0 | 51.4 | 360.0 | 14.9 | 4834.0 | 0.077 | 0.000 | 0.0008 | 0.000000000 |
| 2 | sap_rebuild | 1.956 | 2.161 | 4.396 | 6548.6 | 834.3 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | global_guarded | 1.127 | 2.287 | 3.696 | 10961.4 | 834.3 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | region_fixed | 1.620 | 2.121 | 4.072 | 6548.6 | 834.3 | 379.1 | 2595.9 | 0.0 | 22045.6 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | region_adaptive | 1.560 | 2.169 | 4.062 | 6548.6 | 834.3 | 312.3 | 2662.7 | 0.0 | 23832.7 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | region_sweep_fixed | 1.663 | 2.146 | 4.145 | 6548.6 | 834.3 | 161.7 | 1046.4 | 353.5 | 22045.6 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | region_sweep_adaptive | 1.589 | 2.139 | 4.074 | 6548.6 | 834.3 | 159.4 | 983.6 | 366.8 | 17243.1 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 2 | online_contact_work | 1.604 | 2.150 | 4.091 | 6548.6 | 834.3 | 104.3 | 613.0 | 15.4 | 19969.2 | 0.096 | 0.000 | 0.0014 | 0.000000000 |
| 3 | sap_rebuild | 4.121 | 5.034 | 9.617 | 11523.0 | 2188.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | global_guarded | 2.403 | 5.358 | 8.245 | 21269.1 | 2188.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | region_fixed | 3.086 | 4.985 | 8.619 | 11523.0 | 2188.0 | 815.6 | 5559.4 | 0.0 | 47956.3 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | region_adaptive | 3.001 | 5.032 | 8.613 | 11523.0 | 2188.0 | 649.7 | 5725.3 | 0.0 | 50362.0 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | region_sweep_fixed | 3.117 | 5.026 | 8.698 | 11523.0 | 2188.0 | 305.3 | 1929.7 | 827.8 | 47956.3 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | region_sweep_adaptive | 2.951 | 4.992 | 8.493 | 11523.0 | 2188.0 | 297.4 | 1787.9 | 858.2 | 34463.1 | 0.000 | 0.000 | 0.0000 | 0.000000000 |
| 3 | online_contact_work | 3.155 | 5.004 | 8.714 | 11523.0 | 2188.0 | 176.4 | 871.5 | 27.0 | 40018.7 | 0.096 | 0.000 | 0.0019 | 0.000000000 |

## Interpretation and novelty boundary

The *regional-certified* method divides stable cloth mesh topology into local patches, stores conservative envelope-pair candidate blocks for each patch pair, invalidates only blocks incident to escaping regions, and revalidates cached pairs against current exact padded AABBs. The *adaptive* variant estimates regional movement and observed guard false positives to adjust future budgets. This is a **proposed experimental combination**, not a proven historically novel broad-phase invention.

Broad-phase query time may increase under dense contact because all current candidate pairs are checked and pair-list materialization has a cost. Negative results are valid results. A better policy must beat both SAP and existing dynamic BVH approaches on representative scenes without losing pair coverage.

Compared with the earlier single static sphere, these runs include **genuine changing geometry and inter-sheet positional response**. However, our custom contact handling is discrete and approximate, has no cloth self-collision, and is not a robust scientific reference solver.

## Independent published-library background

- Bullet Physics dynamic AABB trees: https://github.com/bulletphysics/bullet3
- PhysX eABP, ePABP and region broad phases: https://github.com/NVIDIA-Omniverse/PhysX/blob/main/physx/include/PxBroadPhase.h
- Solver-Integrated Deformation-Aware BVH Updates, TVCG 2026, DOI 10.1109/TVCG.2026.3729180
- Efficient Self-Collision Culling for Real-Time Cloth Simulation Using Discrete Curvature Analysis, Mathematics 2026, DOI 10.3390/math14091504

The literature substantially limits novelty claims for local selective updates, temporal reuse, and adaptivity. The prototype is intended to test a cost/selectivity hypothesis with conservative coverage, not to establish a publishable new method by assertion.
