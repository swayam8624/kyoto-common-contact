# Kyoto meeting: corrected result and novelty slides to insert into the presentation

**Use this as a copy/paste Gamma addendum, not as a claim that an old PowerPoint is automatically refreshed.** The tracked \`output/Common_Broad_Phase_Kyoto_Native_v3.pptx\` contains historical figures from before the candidate-only hot-path correction. Update its charts and claims before using it as the final deck.

## Slide A — The real research problem

**Title: The expensive repetition lies between geometry updates and precise contact work.**

Diagram:

Predicted positions / solver correction
→ caller-specific primitive bounds and eligibility
→ broad-phase candidate generation [EXPERIMENTAL RESEARCH TARGET]
→ method-owned geometric narrow phase
→ method-owned contact model / constraints
→ method-owned solver
→ geometry changes → new broad-phase query

**Question:** Can one conservative collision-candidate service safely cache and reuse candidates across solver iterations and eventually across distinct methods without compromising required geometric pairs?

**Research boundary:** a service to *discover* potential pairs, not a new contact manifold, IPC barrier, OGC solver, or CCFR feasible-region algorithm.

## Slide B — Proposed mechanism and proof

**Title: Reuse is permitted by geometry containment, not by guessing motion.**

Left: AABB of one triangle in three versions: raw, margin-padded, and larger stored guard envelope.
Right: pseudocode:

1. Build padded bounds from current geometry.
2. If no cache or request/backend/topology changed, regenerate.
3. Otherwise check \`current_i ⊆ guard_i\` for **every** primitive.
4. If all pass, return cached envelope pair superset.
5. If any fail, rebuild before returning.
6. The method-specific narrow phase decides actual contact.

Core certificate: \`B_i(t) ⊆ E_i ∀i ⇒ C_current ⊆ C_cached\`.

**Caveat:** current discrete AABB queries only; no CCD, no established nonpenetration or cross-method proof.

## Slide C — Published-family algorithm comparison

**Title: Five controlled 3D broad-phase alternatives, tested on identical bounds.**

Insert the *fresh local* figure:
\`output/native/broadphase-leaderboard.svg\`.

Methods:
- Exhaustive eligible AABB overlap pairs (oracle).
- Local sweep-and-prune, full rebuild.
- Upstream Bullet Physics \`btDbvtBroadphase\`, full rebuild.
- Upstream Bullet Physics **persistent dynamic-tree update** (strong temporal baseline).
- Current certified guarded SAP pair-cache reuse.

The figure must display its machine, source algorithm and scenes. These are **broad-phase-only** timings, not IPC/OGC/CCFR or cloth-solver speedups. The benchmark's \`sheets2\` and \`sheets3\` fixtures are synthetic 3D moving mesh bounds; they do not simulate true physically coupled cloth.

**Discuss trade-off:** cache reuses reduce pair-generator calls, but broad envelopes can produce more precise candidates. Compare misses, extra pairs and time, not time alone.

## Slide D — Measured cascade to downstream cost

**Title: Broad-phase savings only matter if the complete contact pipeline benefits.**

Insert \`output/native/cascade.svg\` and optionally
\`output/native/benchmark-postfix.svg\`, **regenerated on your own Mac**.
Chart stage categories should be:

- Broad-phase candidate service.
- Candidate-only precise checks + contact projection.
- Whole-cloth visualization diagnostics.
- Remaining XPBD constraints, integration, bounds construction and bookkeeping.

Also report real **precise test counts** for baseline versus guarded policy. No hypothetical estimates presented as measured physics. The only evaluated real physical fixture is one deforming cloth and one stationary sphere.

## Slide E — What is potentially novel, and what is NOT yet novel?

**Title: A method-aware correctness contract—not a claim to have invented caching.**

**Known:** AABBs, sweep-and-prune, dynamic BVHs, fat bounds and candidate caching.

**Proposed:** A unified interface where each solver explicitly specifies required pair types, proximity offsets, motion interval/CCD bounds, exclusion masks, geometry updates and verification rules; certified reuse with an adaptive safety-preserving envelope budget optimized against **total** contact-stage cost.

**Evidence today:** real same-process custom cloth experiment, independent brute/precise oracles, live motion, paired deterministic positions, upstream Bullet DBVT baseline, verified event logs.

**Missing:** source-level IPC/OGC/CCFR adapters; real multi-cloth solver interactions; validated continuous-time and edge–edge query coverage; broader CPU/GPU and scene sensitivity.

**Closing research question to Kyoto:** Which reference method and contact-query contract should be the first adapter, and what correctness criterion should we preserve?

## Recommended 6-minute live flow

0:00–1:00: Show native cloth and explain why a broad-phase query recurs after geometry changes.
1:00–2:00: Press 2/3/4 for bounds, candidate triangles and actual geometric proximity.
2:00–3:00: Press Q, step N, inspect guard/certificate and cache-decision event log.
3:00–4:00: Press G, scroll inside the right panel to the local paired physics chart.
4:00–5:00: Press H through sparse, clustered, dense, two-sheet and three-sheet *broad-phase* workloads; show actual Bullet 3D dynamic comparator.
5:00–6:00: Scroll to measured downstream cascade and novelty boundary; ask which IPC/OGC/CCFR adapter to implement next.

Do not claim the native 3D demo renders the synthetic multi-sheet benchmark geometry or physically simulates interacting sheets: those benchmark fixtures are geometric inputs evaluated separately by the C++ algorithms. The main native 3D scene remains the actual single-cloth XPBD-style testbed.


## Real moving three-cloth demonstration

Run ./open_multicloth_demo.sh. Keys: L cycles sheet count 1/2/3, O cycles moving sphere count 1/2, Q cycles SAP rebuild/global guarded/regional-certified, A turns exhaustive pair audit on/off, G opens scrollable live event and time-series panel, 1-4 select geometry/guard regions/candidates/actual contact corrections. Insert the newly generated multicloth-real.svg and broadphase-leaderboard.svg charts. The external 3D comparison now includes simulated_cloths3 real physical AABB snapshots. Do not claim the adaptive method is historically novel; assess it using the paired measured cost and zero-miss contract.


## Additional research slides: November 1 hierarchical / granularity ablations

### Slide F — Why global guarded SAP fails as cloth interaction becomes dense

Use the measured 1/2/3-cloth full-physics graph from output/native/multicloth-real.svg. Show the complete CPU timestep alongside broad-phase time and candidate count, explicitly noting that a fast broad-phase cache can be worse overall when conservative extra candidates induce expensive inter-sheet precise contact checks. The physically interacting multi-cloth solver is a custom discrete XPBD-style test fixture, not IPC/OGC/CCFR.

### Slide G — Two-level certified candidate frontier (experimental)

Top-level: sweep sorted UNION AABBs of stable mesh patches, omitting disjoint patch pairs. Second level: certify each patch's per-primitive envelope; rebuild only pair-cache blocks incident to invalidated patches; then check stored primitive candidates against current tight padded AABBs. Show the conditional proof: if each current padded primitive AABB is inside its stored envelope, then every current required overlap pair must be in the envelope candidate set. A disjoint union-envelope pair cannot contain any current required overlap. The design is related to existing region broad phases, not an established novel invention.

### Slide H — Why we measured patch size instead of inventing an optimal number

Use the measured output/native/patch-sweep.svg chart. Its 12 experiments use 1, 2, 3, 4, 6 and 12 cloth cells per region, with flat and hierarchical patch-frontier modes. Select one policy using earlier calibration frames ONLY; report its performance on later evaluation frames, including SAP as a fallback. Explain that a fine 1x1 partition can be very expensive despite pruning thousands of disjoint patch pairs. Highlight both positive and negative cases.

### Slide I — Novelty decision gate

**Implemented:** verified partial region invalidation, hierarchical envelope-pair culling, tight candidate revalidation, variable topology-aligned region granularity, static profile-based configuration, actual inter-sheet dynamics, independent Bullet/DPVT comparisons, per-frame physical state agreement with SAP. **Not implemented:** online optimum-granularity learning, full continuous collision detection for cloth, self-contact, first-party IPC/OGC/CCFR adapters, GPU LBVH and PhysX ABP/PABP comparisons. Do not label mere regional spatial subdivision, hierarchy, guards, or patch profiling a new published broad-phase algorithm. Research novelty requires related-work differentiation and a convincing advantage across independent scenes and hardware.

In the native viewer: launch open_multicloth_demo.sh, press J to compare all-pairs versus two-level region frontiers, P to change patch size, Y to apply the complete locally measured calibration matrix (or SAP fallback), and G to scroll to the six-way physics-step comparison and live patch-pair counts.
