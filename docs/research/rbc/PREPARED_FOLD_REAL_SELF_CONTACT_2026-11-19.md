# RBC Stage 2 — 121,376 genuine prepared-fold physical self-contacts

Native physical study: 19 November 2026.
Status: true source-backed five-policy XPBD-style coupled simulation;
NOT calibrated FEM, general CCD, independent penetration-free guarantee,
spontaneously arising cloth folds, or statistically proven superiority.

## Original independently verified native source evidence

Initial five-policy real 80-frame native experiment:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44549527676

Original ZIP, artifact 14541892868, SHA256:
10efbc4408c77683bcb238bca4ec74acff23ebb7212efac5a525328cb3ca0725

Original three solver CSV source receipts independently SHA256 verified,
all matching native PREPARED_TRUE_SELF_CONTACT_RESULTS.json.

Original actual full mesh/sphere JSONL export independently verified in:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44550735953

Five policy source JSONL files include original triangles, topology,
initial and per-frame real vertex coordinates, two actual sphere centers
and radii, full-step times, broad times, and original self-contact
counters. That CI verified five complete 80-frame trajectories,
identical SAP positions up to 1e-7, same original topology,
and matching real self-contact event counters. Artifact SHA256
899b0d92c8e4b339a61c75cc992f465f4c36af201be4457ea99f849a0f55eb3e.

## Critical distinction: prepared geometry vs naturally folding cloth

The original flat cloth is deliberately PRE-FOLDED at time zero:
the true lower flap of the first 13x13 mesh reflects about its
central row into its own upper half. Original spring rest lengths,
particle connectivity and collision response remain unchanged.
The resulting real overlapping mesh is then advanced physically
for 80 frames under each of the five original independently
advancing collision-candidate engines.

The previous continuously driven, nonteleporting barycentric
150-frame fold_over from a flat starting cloth, by contrast,
generated ZERO natural same-sheet self-contact corrections.
That negative result is not erased. Original run:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44548181162

## Measured prepared-fold native results

Each one of three physical cases has 80 real solver frames,
five source collision queries/step and separate trajectories
for rebuilt SAP, Guard, frozen V4, old indexed RBC and flat RBC.
All source positions agree with SAP within 1e-7, and real
sphere and self contact counts agree across policies.

| Geometry/topology seed pair | Real same-sheet contact corrections | Discrete self-crossing detections | Guard/flat RBC full-step ratio |
|---|---:|---:|---:|
| 73013 / 86011 | 42,831 | 13,431 | 1.08900x |
| 74017 / 86019 | 34,014 | 12,154 | 1.05218x |
| 75029 / 86029 | 44,531 | 13,660 | 1.10038x |
| TOTAL / 3-case geomean | 121,376 | 39,245 | 1.08032x |

Count each unique physical case ONCE; do not multiply
its contact-count by five identical source-policy arms.
The self_contacts and self_crossings are genuine
internal native discrete solver counters, but their
correctness does not establish a fully independent
external penetration or CCD ground truth.
One timing session per three prepared source variants
cannot imply significance across general physical solvers.

The earlier 54-scene broad-phase AABB query test
still shows Guard/flat geometric ratio 0.87099x,
so RBC FLAT is slower overall for equally weighted
query-only scenes; the cushion family remains a
severe negative control. The 9 original coupled
scenarios have only 1.02656x Guard/flat full-step
geomean, zero natural same-sheet self-contact.
All positive and negative evidence must be retained.

## Native original 3D continuous source render

The coupled native exporter accepts an optional output
directory for five genuine mesh frame JSONL streams:

    build/kyoto-final-cpu/ccp_rbc_coupled_tournament \
      output/rbc-prefold-mac.csv 80 prefold_relax 73013 86011 0 \
      output/rbc-prefold-mac-realmesh

The renderer reads only these original source JSONLs,
checks complete topology and exact per-frame state
parity, then emits a continuous looping 5-panel
same-camera 3D GIF, one PNG proof preview,
and a JSON provenance manifest of each actual
native source SHA256:

    python3 scripts/render_rbc_physics_grid.py \
      --mesh-dir output/rbc-prefold-mac-realmesh \
      --output output/rbc-prefold-mac-five-policy.gif \
      --stride 3 --fps 12

All five valid candidate generators MUST show the
same physical geometry; do NOT invent improved folding
from a mere exact broad-phase replacement.

## Research gates remaining

Induce real nonlocal self-fold from an initially
non-overlapping flat cloth WITHOUT prepared starts.
Validate independent triangle penetration/CCD and
solver strain/energy/contact consistency.
Repeat across novel meshes, materials, size regimes
and multiple independent measurement sessions with
honest A/A; compare to tuned fixed margin Guard,
published IPC Toolkit LBVH, HashGrid and native
solver contact logic with the same semantics.
Then and only then pursue broader full-step
and novel scientific contributions.
