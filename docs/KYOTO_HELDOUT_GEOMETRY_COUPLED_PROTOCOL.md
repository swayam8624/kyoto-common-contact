# Kyoto — independent geometry/topology and coupled-contact research gate

Status: implemented as an ADDITIVE research track on
research/kyoto-decision-coverage. Do not replace historical Phase 18
Mac A/A data, retune the frozen V4 selector, or confuse AABB-only
material snapshots with end-to-end solver physics.

## Scientific motivation

The 144-replay Phase 18 repeated Mac natural suite yielded a
Guard/frozen-V4 query-time geometric mean of 1.0188x on the original
36 material trajectories, concentrated in the rapid-cloth N=1058
condition (1.2139x). Eleven other conditions averaged approximately
1.0027x when that strong fixture was excluded. Those tests varied
only temporal forcing phase within the same material geometry and
mesh connectivity. They are not evidence of general new-scene
performance or physical contact quality.

This new work explicitly separates two scientific questions:

1. **Does a frozen controller maintain exact AABB candidates and useful
   speed on freshly generated material geometry and new connectivity?**
   Opt-in independently seeded material-world variation changes node
   positions, genuine cloth triangle diagonals, cloth anchoring, and
   volumetric cross-bracing. The new worlds run their real damped Verlet
   structural solver and are exported as immutable KYOTOM01 3D AABB
   sequences. All original actual full-pair methods process exactly
   the same stream; comparisons are matched against full SAP
   count/hash every frame. C++ unit tests additionally use an
   independent full pair-list brute-force oracle. This track is
   discrete candidate detection ONLY, not a coupled narrow-phase
   benchmark and not a calibrated continuum material model.

2. **Does the original policy change the outcomes of an existing
   contact-capable solver under different actual movements?**
   The new ccp_kyoto_heldout_coupled executable exercises the
   independently stepped MultiCloth XPBD-style constraint/contact
   solver with three UNMODIFIED original policies: fresh SAP,
   Global Guard and frozen V4. Scenarios are free motion, a
   barycentric pulled cloth patch, and a pulled-inward fold.
   Each independent solver performs actual physics and emits
   per-frame full-solver time, broad-phase time, narrow-phase time,
   contact counts, physical strain and degenerate triangles.
   Complete vertex positions are compared with the SAP solver
   each frame to a 1e-7 tolerance; missed broad-phase candidate
   assertions abort. It also writes truthful mesh JSONL usable
   in synchronized visual grids.

   Crucial limitation: MultiCloth itself has a fixed grid
   triangulation. The independent-topology variations in (1)
   have NOT been integrated into MultiCloth. It is therefore
   incorrect to claim simultaneous new topology AND independent
   coupled collision physics from a single experiment.

## Backward compatibility

World now accepts an optional Variation, default-initialized with
both geometrySeed and topologySeed zero. All original Phase 18 calls
with four constructor arguments preserve the historical cloth
TWO-diagonal spring structure and the original a-d triangle split.
The new generator is opt-in only.

The existing ccp_material_export CLI, all original six/seven-argument
calls and KYOTOM01 binary ABI remain unchanged. To opt in to
independent variations, explicitly provide BOTH additional seeds:

    ccp_material_export OUTPUT.kmcb MATERIAL BUDGET FRAMES MOTION_SEED REGIME GEOMETRY_SEED TOPOLOGY_SEED

There is no silent reinterpretation of Phase 18 motion seeds.
New coordinates/connectivity depend on new seed pairs, rather than
being rebranded forcing-phase variants. The new C++ regression
checks independent seed effects, deterministic replay, historical
zero-variation compatibility, mesh validity and exact current-AABB
pair oracle results across six material families.

## Full runnable Mac/Ubuntu experiment

From the repository root on research/kyoto-decision-coverage:

    git pull --ff-only origin research/kyoto-decision-coverage
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_export ccp_material_replay ccp_kyoto_heldout_fixture_tests ccp_kyoto_heldout_coupled
    build/kyoto-final-cpu/ccp_kyoto_heldout_fixture_tests

Run the full **new-geometry** matrix with a NEW directory; this
creates three independently topologized/perturbed fixtures for
each of cloth, sponge, mixed at both 128 and 512 budgets and both
baseline and rapid conditions. It does NOT reuse original Mac
Phase 18 snapshots. The coupled audit separately runs free,
pull and fold with two cloth sheets and two spheres:

    python3 scripts/run_kyoto_heldout_research.py --exporter build/kyoto-final-cpu/ccp_material_export --replayer build/kyoto-final-cpu/ccp_material_replay --coupled build/kyoto-final-cpu/ccp_kyoto_heldout_coupled --output output/kyoto-heldout-original-01

    python3 scripts/test_kyoto_heldout_research.py output/kyoto-heldout-original-01

    open output/kyoto-heldout-original-01/HELDOUT_RESULTS.md

All source files are immutable by policy: the runner refuses to
overwrite an existing output directory, raw source snapshot and
replay files have SHA256 receipts, and the coupled mesh exports are
audited against the native SAP-driven solver.

The full output has JSON and Markdown manifests, complete per-policy
replay CSVs, all new 3D snapshot frames, process logs, and per-scenario
coupled solver CSV/JSONL. A complete one-shot speedup is descriptive:
there are no invented confidence intervals, throughput multipliers,
or performance claims before the native Mac runs.

## Visual grids and synchronized contact motion

The existing mesh renderer accepts the new per-policy JSONL directly:

    python3 scripts/render_kyoto_visuals.py solver --input-dir output/kyoto-heldout-original-01/coupled/fold --output output/kyoto-heldout-original-01/coupled/fold-visual --fps 9 --stride 1

Repeat for coupled/pull and coupled/free. Requires Pillow; ffmpeg is
optional. It creates a looping GIF and optional MP4 with synchronized,
real simulation vertices and measured contact diagnostics, NOT
decorative, physically altered meshes. Every panel is supposed
to look the SAME for an exact broad phase. Any changed folding
would be a correctness defect, not a free visual quality gain.

## Exactness and interpretation

The new snapshots encode floats and stable AABB IDs, but do not
contain triangle connectivity; the independent-topology evidence
comes from the native World fixture tests and the generator code.
No claim is made that the serialized AABB file alone proves an
alternate mesh topology. Comparisons use all current broad-phase
candidate pairs and exact SAP count/hash parity. There is also a
separate complete brute-force small-scene pair oracle.

The existing solver diagnostics count narrow-phase tests and
contacts and compare full vertex geometry. They do NOT measure
an independently validated analytic penetration-depth oracle,
continuous collision detection, true cloth tearing/remeshing,
or a calibrated continuum constitutive law. Some coupled solver
metrics are recorded but not symmetrically interpreted as
performance speedups: the three policies are stepped in fixed
separate sessions rather than balanced ABBA timing runs.

Success criteria for this stage are (1) truly different seed-based
initial geometry and connectivity, (2) exact AABB pair parity under
the untouched selector, (3) complete contact solver geometry parity
and physical diagnostics across policies, and (4) reproducible
cross-platform C++ regression evidence. Any actual speedup can
be stated only after the new native runs complete and the actual
numbers are reviewed. No broad Phase 18 victory is assumed.
