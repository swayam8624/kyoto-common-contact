# Kyoto common contact — source reading order and cross-file execution map

This repository is a native C++23 *research prototype*, not a production-ready collision library or a proof of intersection-free cloth. Start with [the worked broad-phase and XPBD laboratory](BROADPHASE_TO_CONTACT_TEACHING_LAB.md), then study the actual sources in the sequence below. Keep the historical single-cloth path, current multi-cloth path, viewer, and benchmark harness separate when evaluating claims.

## 1. Problem, evidence, and build

First read [README](../README.md), [CMakeLists.txt](../CMakeLists.txt), [the online algorithm](ONLINE_ALGORITHM.md), [multi-cloth contract](MULTICLOTH_CONTRACT.md), [core audit](CODE_AUDIT_CORE.md), [evidence methods audit](EVIDENCE_METHODS_AUDIT.md), and [native reproduction cases](NATIVE_CASES.md).

CMake creates multiple independent targets. The headless libraries are ccp_live from live_cloth.cpp, ccp_multicloth from research_multicloth.cpp plus online_partitioner.cpp, and ccp_core from core.cpp. The native GLFW viewer consumes ccp_multicloth on macOS. Bullet DBVT is an **external optional comparator** when found; merely having an executable named broadphase_leaderboard does not imply the independent library is present.

## 2. Start at the public contracts

Read [include/ccp/live_cloth.hpp](../include/ccp/live_cloth.hpp), [include/ccp/research_multicloth.hpp](../include/ccp/research_multicloth.hpp), and [include/ccp/online_partitioner.hpp](../include/ccp/online_partitioner.hpp).

Identify Bounds with stable primitive ID, min/max, category mask, mobility, QueryRequest with margin/guard budget/policy, Pair with canonical unordered IDs, the four Policy variants, and Metrics. These types are the *interfaces* between algorithms. A candidate Pair means only "send this potentially overlapping pair to a more expensive stage"; it is not a resolved physical contact.

The Metrics::stepMs value measures the complete CPU simulation step, including enabled audit work and online policy decisions, but excludes native GPU rendering. Metrics::broadMs counts query work; precise/solver work is distinct. Never present one of these numbers as if it were the other.

## 3. Baseline candidate generation and global temporal coherence

Read [src/live_cloth.cpp](../src/live_cloth.cpp). Begin with live::candidates, the isolated brute-force-versus-SAP primitive AABB search. The SAP implementation sorts by minimum X, terminates once a later box starts past the current maximum X, then tests mask eligibility and the full three axes. A brute-force reference checks all eligible unordered pairs.

Continue with BroadPhaseService::query. Current geometry is padded by half the requested margin. A global fat-envelope cache may reuse previously generated candidate pairs only while the request, backend, primitive count and *per-index full metadata plus geometry containment* remain valid. Reused lists can contain extra pairs, so the consuming MultiCloth::projectContacts must recheck current padded bounds before issuing positional corrections.

Study [tests/live_cloth_tests.cpp](../tests/live_cloth_tests.cpp) to distinguish candidate inclusion, duplicate IDs, invalid bounds, request invalidation and cache behavior. The older ClothSimulation in this file is a historical single-cloth fixture, not the same physics case as the current three-cloth experiment.

## 4. Local certified candidate cache and the true cloth simulator

Read [src/research_multicloth.cpp](../src/research_multicloth.cpp) in four phases.

First, RegionalCandidates::query verifies unique primitive IDs, finite and noninverted AABBs, and request/region signatures. It groups primitives by region; for each region it checks whether every current padded box is enclosed by its previously stored guard. A dirty region regenerates guard envelopes and affected cached pair blocks.

Second, top-level hierarchical sweep compares region union envelopes. It skips disjoint region pairs without losing valid primitive-pair coverage, assuming guards and memberships were correctly maintained. Flat enumeration is the alternative. Both ultimately resolve primitive IDs and **revalidate cached pairs against current padded tight bounds**. If an old guard pair no longer overlaps, it should not be sent to the contact stage.

Third, MultiCloth::makePrimitives constructs triangle and moving-sphere AABBs from current positions, collision categories and regions. MultiCloth::step performs fixed-dt Verlet integration, constraints, floor and mouse attachments, calls one selected broad-phase policy on each solver iteration, and passes resulting candidate pairs into projectContacts.

Fourth, projectContacts computes approximate discrete geometric contacts. It processes triangle-sphere and triangle-triangle feature pairs, self-contact topological exclusions and edge-face piercing checks, then applies bounded inverse-mass-weighted positional corrections. Previous-position adjustment limits artificial Verlet velocity. This is **not CCD** and does not guarantee self-intersection-free motion for arbitrary cloth folds.

The periodic exhaustive AABB oracle can assert candidateSet includes the oracle's pairs. The more stringent regional path also checks equality under the sampled fixtures. Neither gives a temporal time-of-impact proof.

For grounded tests use [tests/research_multicloth_tests.cpp](../tests/research_multicloth_tests.cpp).

## 5. Online controller: change partitioning without changing physics

Read [src/online_partitioner.cpp](../src/online_partitioner.cpp). OnlinePartitioner::reset seeds four 6x6 cell patches per cloth. stableId includes sheet, patch level and lattice origin. regionForCell maps each mesh cell to exactly one patch; a missing patch would be a candidate-coverage bug.

OnlinePartitioner::observe uses actual AABB work, cached candidates, invalidations and contact corrections to update smoothed work/contact estimates. It splits sufficiently hot patches into four children or merges four quiet siblings. Decision windows, repeated thresholds and cooldowns prevent immediate split/merge oscillation. It may switch between a flat region pair enumeration and a top-level sorted axis sweep based on occupancy and smoothed measured query times.

After the full MultiCloth::step, the decision updates next-frame region assignments. Any changed region IDs or traversal frontiers invalidate the existing RegionalCandidates cache. Online feedback does not supply independent future collision ground truth, and its adaptive decisions are not proof that this controller is a globally optimal policy.

Check [apps/online_controller_trace.cpp](../apps/online_controller_trace.cpp) for measurements and paired solver trajectories. The controller trace is more relevant to *closed-loop multi-cloth behavior* than an isolated AABB timing leaderboard.

## 6. Viewer, mouse picking and rendering

Read [include/ccp/cloth_interaction.hpp](../include/ccp/cloth_interaction.hpp) and then [native/main.cpp](../native/main.cpp). The helper builds a camera ray from GLFW window cursor coordinates, performs two-sided nearest-triangle ray selection and projects subsequent drag rays into a camera-facing plane. The resulting barycentric attachment is handled by MultiCloth::beginGrab/updateGrab/projectGrab, which enters the existing solver rather than directly teleporting triangle vertices.

The viewer's OpenGL renderer displays positions, geometry, bounds, candidate/contact overlays and metrics. Rendering is optional for headless proof of pair coverage; a screenshot does not validate broad-phase completeness. On Retina displays, cursor window dimensions and framebuffer pixel dimensions differ; picking must use a consistent coordinate system.

## 7. Benchmark and evidence code

Read [apps/multicloth_benchmark.cpp](../apps/multicloth_benchmark.cpp), [apps/online_controller_trace.cpp](../apps/online_controller_trace.cpp), [apps/patch_granularity_benchmark.cpp](../apps/patch_granularity_benchmark.cpp), [apps/cloth_stability_probe.cpp](../apps/cloth_stability_probe.cpp), and [apps/broadphase_leaderboard.cpp](../apps/broadphase_leaderboard.cpp) when the optional Bullet target is available.

Read [scripts/prepare_meeting.sh](../scripts/prepare_meeting.sh), [scripts/test_evidence_summarizers.py](../scripts/test_evidence_summarizers.py), and [docs/EVIDENCE_METHODS_AUDIT.md](EVIDENCE_METHODS_AUDIT.md) for the exact evidence chain.

A fair benchmark records primitive count, cloth count, contact density, solver policy, candidate completeness, total physical step time, query-only time, audit policy, warmup, seeds, platform/toolchain, source SHA and repeated variation. Paired runs must use equal initial physical state and equivalent solver settings. Published method names require actual upstream implementations; the optional Bullet DBVT target is not permission to claim PhysX, IPC or OGC were integrated.

## 8. Worked learning exercises

[The accompanying laboratory](BROADPHASE_TO_CONTACT_TEACHING_LAB.md) derives guard-envelope correctness, a concrete false-positive reuse case, the EWMA patch update, a barycentric grab and a pair-coverage oracle by hand. Work it after the public headers and before browsing the larger 1000-line simulator.

The most important mental distinction is:

**candidate completeness ≠ physical contact correctness ≠ intersection-free temporal simulation ≠ runtime speedup.**

Tests and visualizations contribute evidence for different parts of that chain. Preserve failed and inconclusive experimental results and label the exact workload actually measured.

*This is a linked reading curriculum. It does not claim every function in this repository is already annotated or that a new native benchmark has been executed.*
