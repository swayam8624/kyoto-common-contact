# Prior-art controls for a genuinely new contact-broadphase claim

**Research literature screen — 23 November 2026**

This note deliberately prevents rediscovering an established BVH
algorithm and calling it novel. URLs are primary publisher/author
sources. The screen is **not** a completed exhaustive systematic
literature review; verify full-text algorithms before claiming
differences or publication novelty.

## Directly relevant prior art

| Work | Demonstrated published contribution | Consequence for Kyoto |
| --- | --- | --- |
| Tang, Manocha, Tong, *Multi-core collision detection between deformable models*, ACM SPM 2009, DOI 10.1145/1629255.1629303 | Incremental hierarchy computations, construction/updates of BVTT traversal fronts, parallel triangle contact tests | **Retaining/updating a BVTT front is established prior art**, and cannot be our main novelty |
| Tang, Manocha, Tong, *MCCD: Multi-core collision detection between deformable models using front-based decomposition*, *Graphical Models*, 2010 | Incremental continuous-collision front tracking, deformable self-collision, 2–3 tree for BVTT front, orphan-set/adaptive front updates | Our first retained cut (binary BVH, sequential CPU) is strictly less general; a bounded cut alone needs a stronger distinguishing claim |
| *Scalable collision detection using p-partition fronts on many-core processors*, IEEE TVCG, DOI/PubMed 24434225 | Partitioned traversal fronts with explicit parallel load balancing and reduced memory overhead | Memory-bounded contact-front maintenance is **not generically unprecedented**; compare memory, work and update strategy precisely |
| *Interactive Continuous Collision Detection for Topology Changing Models Using Dynamic Clustering*, 2015 | Continuous detection on triangulated deformables with changing topology and dynamically updated clusters | Resetting our tree when topology changes is a safety measure, **not a new topology-changing CCD result** |
| IPC Toolkit, `ipc-sim/ipc-toolkit`, v1.5–v1.6 releases (2026) | Public `ipc::LBVH` broadphase, official conservative narrowphase CCD, default LBVH in v1.6 | Pin the exact commit and compare complete paid conversion/build/query, not a homemade stand-in |
| IPC Toolkit API and tutorial documentation | Exposes full solver-independent broadphase/CCD interface with both linear and nonlinear trajectory workflows | Our current source feature comparison is candidate-only; independent event/physics validation remains distinct |

Primary sources:

- ACM SPM 2009: https://doi.org/10.1145/1629255.1629303
- MCCD journal abstract: https://www.sciencedirect.com/science/article/abs/pii/S1524070310000020
- MCCD author-hosted version: https://gamma-web.iacs.umd.edu/PCD/pccd.pdf
- P-partition fronts: https://pubmed.ncbi.nlm.nih.gov/24434225/
- Dynamic clustering (open full text): https://pmc.ncbi.nlm.nih.gov/articles/PMC4500942/
- IPC Toolkit upstream: https://github.com/ipc-sim/ipc-toolkit
- IPC release notes: https://github.com/ipc-sim/ipc-toolkit/releases

## What we can and cannot currently distinguish

**Established already:** Binary BVH refits, temporal coherence,
front tracking, adaptive front refinement, self-collision queries,
parallel front decomposition and topology-aware rebuilding. Our
previously named "Spacetime Contact Atlas" is **not automatically
new** merely because it maintains a frontier or uses a different name.

**Our measured, narrower engineering result:** The same source
3D neo-Hookean FEM snapshots produce exact typed VF/EE candidate
agreement against an exhaustive oracle, with a conservative,
preemptively budget-limited node-pair product cut. The cut keeps
coarse overlapping products and **fully traverses** them rather
than discarding an oversized frontier after paying its generation
cost. The observed benefit over a plain cached hierarchy has been
small and workload-specific in the tested data.

**Critical apples-to-apples threat:** The original upstream IPC
adapter reconstructed metadata on every frame, while the maintained
tree reused it. A second comparator now reuses the upstream
Eigen/feature topology exactly as the maintained tree does while
charging current swept-box updates. Until its CI receipts are
verified, a `1.5×` ratio against the original adapter does **not**
establish algorithmic superiority.

**Physics-specific limitations:** Source includes multiple real
tetrahedral resolutions/stiffnesses but only one internally written
FEM solver. The accepted 20 m/s adaptive leaf CCD audit yielded
zero actual upstream CCD events (safe source leaves); an unaccepted
solver-native full-dt proposal is separately audited to require
nonzero actual events and complete candidate coverage. This is not
independent CCD mathematics.

## Required decision before publication

A paper-level differentiator should be formulated as a measurable
question such as:

> Under identical real deforming source trajectories and precise
> VF/EE filtering semantics, can a bounded, conservative,
> selectively maintained product frontier reduce *total paid*
> collision detection cost over official, equally cached IPC
> LBVH and established front-tracking approaches, with no actual
> CCD event misses and controlled memory/worst-case behavior?

The claim fails if:
- the equally cached IPC baseline wins;
- broadphase speed gains disappear once actual upstream CCD runs;
- a solver-integrated contact process is unchanged or slower;
- workloads with dense contact destroy any maintenance benefit;
- important features, masks or floating-point cases miss events;
- an earlier paper already implements the same update policy.

Promising alternatives are **not yet results**: dynamic bounded-frontier
coarsening based on measured work, certified temporal interval
pruning, source-leaf event scheduling, and selective local refit.
Each requires both direct prior-art diligence and measurable
ablation before novelty language is warranted.

**Do not publish, claim novelty, or market general superiority from
the present benchmark suite alone.**
