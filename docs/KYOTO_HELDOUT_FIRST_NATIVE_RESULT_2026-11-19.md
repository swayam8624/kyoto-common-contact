# Kyoto held-out native geometry and coupled collision result (Linux CI, 19 November 2026)

The source of the following measurements is the actual 48-frame CI evidence artifact from
[GitHub Actions run 44465880693](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44465880693).
The physical outputs and full-contact metrics were inspected directly from the original
HELDOUT_RESULTS.json plus all coupled per-policy JSONL files. This is Linux CI evidence,
not a new Apple Silicon result.

## Independent physical meshes

Two opt-in, independently generated solver fixtures were evaluated:

| Mesh | Vertex geometry seed | Triangle/constraint topology seed | Anchored vertices |
|---|---:|---:|---:|
| A | 73013 | 86011 | 4 |
| B | 74017 | 86019 | 6 |

The original coupled solver still uses a 13x13 rectangular cloth grid family,
but source geometry, cell diagonals, structural constraint edges and
anchoring vary under separate seed controls. Exact source initial mesh
coordinates and triangle-connectivity hashes were rechecked independently
across variants. Within each particular source mesh, SAP, Global Guard,
and frozen V4 produced identical physical vertex trajectories to 1e-7,
the same contact counts and same precise-test counts. Original policies
and frozen V4 parameters were not changed.

## Genuine narrow-phase/contact work, not just empty candidate-set parity

Each result below sums 48 physical frames with five solver substeps per
frame. These are actual physical solver contact response events, not
unique physical collision pairs or externally validated physical ground truth.

| Scenario | Mesh | Precise narrow-phase tests | Sphere-contact responses | Nonlocal cloth self-contact responses | Max collapsed triangles | Frozen V4 actual Guard / SAP / trial / switch queries |
|---|---|---:|---:|---:|---:|---|
| Free | A | 347,227 | 5,014 | 0 | 0 | 240 / 0 / 0 / 0 |
| Grab-fold | A | 343,785 | 5,470 | 0 | 0 | 240 / 0 / 0 / 0 |
| Free | B | 363,710 | 4,942 | 0 | 0 | 240 / 0 / 0 / 0 |
| Grab-fold | B | 352,487 | 5,256 | 0 | 0 | 240 / 0 / 0 / 0 |

**Negative result: NO self-cloth contact response occurred on these
trajectories.** The moved mesh can fold geometrically without generating
an actual nonlocal cloth self-collision event in these 48 frames.
The sphere-contact and precise-test evidence is real and substantial.
Self-collision correctness on meaningful folds is NOT established.

**Negative result: V4 never switched to SAP in the coupled solver.**
The actual instrumented V4 method chose Guard for all 240 broad-phase
queries in EACH 48-frame physical case; there were no bounded trials
and no transitions. We therefore have demonstrated coupled-contact
state parity with SAP and Guard, NOT an adaptively faster coupled
simulation. Earlier Phase 18 rapid-cloth AABB-only SAP activation
was a different fixed-fixture cohort and cannot be pooled here.

## Descriptive actual full-solver timings, NOT a performance win

| Scenario | Mesh | SAP mean full step (ms) | Global Guard mean full step (ms) | Frozen V4 mean full step (ms) | Maximum RMS edge strain |
|---|---|---:|---:|---:|---:|
| Free | A | 4.5680 | 3.6647 | 3.5905 | 0.01886 |
| Grab-fold | A | 4.6061 | 3.6355 | 3.5936 | 0.02192 |
| Free | B | 4.7191 | 3.5343 | 3.5175 | 0.01772 |
| Grab-fold | B | 5.0537 | 3.7694 | 3.7246 | 0.02247 |

The three policies were stepped sequentially in a single Linux CI
session. Temperature/cache/CPU frequency/order effects are not
balanced. These data are source-backed descriptive timings only,
without a population CI, independent physical reference material,
CCD oracle or verified penetration-free geometry.

## Synchronized mesh visualization

The raw artifact contains actual per-policy mesh headers, 576
triangles per two-sheet scene, 338 physical nodes and 48 genuine solver
frames. A synchronized 2-mesh-by-3-policy looping GIF can be made
from these exact snapshots. **All policies within a row SHOULD look
identical.** Inventing reduced folding or attractive altered
deformation for one exact-candidate policy would be scientific fraud.
The only actual geometric differences are between independently
seeded mesh A and mesh B.

## Reproducibility and next gate

Native source:
- include/ccp/material_scenes.hpp
- include/ccp/research_multicloth.hpp and src/research_multicloth.cpp
- apps/material_snapshot_export.cpp and apps/kyoto_heldout_coupled_audit.cpp
- tests/kyoto_heldout_fixture_tests.cpp and tests/kyoto_coupled_variation_tests.cpp

Experiment and fail-closed auditing:
- scripts/run_kyoto_heldout_research.py
- scripts/test_kyoto_heldout_research.py
- docs/KYOTO_HELDOUT_GEOMETRY_COUPLED_PROTOCOL.md

Next scientific objective: distinct physically meaningful mesh families,
nonlocal self-contact scenarios with verified responses and reference
penetration/CCD checks, and balanced full-solver comparative timings
where frozen V4 REALLY decides between Guard and SAP.
The original Phase 18 Mac evidence and frozen selector remain unchanged.
