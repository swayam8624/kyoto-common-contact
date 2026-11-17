# CBP Phase 5 controlled four-span topology ablation — negative result

Date: 17 November 2026. Source: GitHub Actions run https://github.com/swayam8624/kyoto-common-contact/actions/runs/44149127299 (Ubuntu CPU).

**Scientific status: exploratory developer benchmark, not held-out performance validation or complete-solver speedup.**

## Exact same physical geometry, different topology patch ownership

- Two cloth sheets (288 triangles each), one sphere (total 577 primitives), 24 frames from the existing coupled `MultiCloth` reference simulation.
- Four fixed topology cell spans: 1, 3, 6, 12. Measured after first 3 frames, giving 21 timed frames per span.
- Every algorithm on every frame returned the SAME exact sorted candidate pairs as fresh SAP; independent brute-force spot checks passed.
- Per-frame full geometry fingerprints matched across all four spans. Actual triangle-to-sheet/cell and sphere region ownership was verified. This eliminates the possibility that different geometry was accidentally fed to the different patch-span comparisons.
- There were **105,111 total candidate-pair occurrences across the 24-frame trajectory** at every span. This number is a sum of repeated frame candidate counts, not unique unordered pairs.

## Measurements (mean per full broad-phase query, milliseconds)

| Method | Span 1 (289 regions) | Span 3 (33 regions) | Span 6 (9 regions) | Span 12 (3 regions) |
|---|---:|---:|---:|---:|
| Frozen V4 (Guard/SAP bounded trial) | **0.177608** | **0.180186** | **0.180312** | **0.181473** |
| Regional exhaustive | 0.498523 | 0.232107 | 0.354631 | 0.329683 |
| Regional shared BVH | 0.410649 | 0.346219 | 0.364944 | 0.354823 |
| Regional certified graph | 0.811075 | 0.522606 | 0.478157 | 0.433593 |
| Phase-3 causal regional controller | 0.503578 | 0.351486 | 0.354703 | 0.337097 |
| Phase-4 V4/regional portfolio | 0.251764 | 0.205838 | 0.230021 | 0.229207 |
| Rebuilt SAP | 0.312325 | 0.313164 | 0.309293 | 0.316029 |

The Phase-4 portfolio **rejected** one four-frame regional trial in every span and spent 17 of 21 measured frames on frozen V4; no trial was accepted. Its ratio to frozen V4 ranged from 0.705x to 0.875x, meaning it was slower despite falling back. The full-scan regional exhaustive kernel was fastest among regional baselines only at span 3, but still slower than V4.

All seven-method CMake tests, geometry-parity tests and sanitizers passed in the associated GitHub Actions workflow. This confirms the tested implementation's exactness, not its performance generalization.

## What this DOES and DOES NOT falsify

**Falsified on this test:** the claim that an ordinary topology patch partition plus the currently implemented RegionRouter kernels gives a broad-phase speedup over V4; the claim that a simple V4-versus-regional portfolio can recover such a speedup here. More regions can add substantial management overhead, and fewer/bigger regions can cause expensive local candidate enumeration.

**Not falsified:** the more ambitious hypothesis that a genuinely region-local, work-budgeted maintenance representation can improve heterogeneous/contact-rich scenes. This current implementation DOES NOT share the expensive global/local spatial maintenance state in a way that avoids recomputation, or allocate independent certificate/SAP/BVH maintenance policies inside separate regions. No representative dynamically mixed workload was tested.

**Not proven:** any SIGGRAPH/TOG novelty, solver stability improvement, reduced penetration, CCD guarantee, memory scaling or real-garment performance. The cloth solver only supplies snapshots for a standalone query replay; its `stepMs` measures the reference policy, not integration under the new algorithms.

## Researcher's next diagnostic

Do not retune thresholds to recover these same inspected 21-frame means. Profile the region ownership/map construction, per-region local checks, cross-region traversal, dynamic certificate maintenance, allocation, and output sorting separately **without perturbing the timed benchmark**. Then design an indexed *local* scheme that removes a measured bottleneck, with a conservative exact Guard/V4 escape. Acquire separate unseen workloads with genuinely different motion/contact regimes occupying different parts of one scene. Keep these results as fixed development diagnostics and run new pre-registered confirmation cohorts only after architecture changes.
