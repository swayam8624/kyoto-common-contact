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

   Independent geometry and topology seeds are now integrated into
   MultiCloth itself. The optional new fixture changes TRUE triangle
   adjacency, structural spring edges, anchor constraints and initial
   vertex positions BEFORE contact simulation. Three policies step
   independent solver instances from the same particular new mesh.

   Remaining limitation: MultiCloth still has a 13x13 rectangular
   mesh lattice as its underlying parameterization. The seed changes
   diagonals/geometry/constraints but does NOT implement arbitrary
   remeshing, tearing, subdivision, FEM or independently measured
   physical material parameters. These are multiple real topologies
   within one mesh family, not arbitrary topology classes.

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
being rebranded forcing-phase variants. The new C++ regression checks independent seed effects, deterministic
replay, historical zero-variation compatibility, mesh validity and
exact current-AABB pair oracle results across six material families.
A separate native coupled solver regression verifies that geometry
seeds independently change actual solver vertex positions, topology
seeds independently change triangle adjacency, and SAP, Guard and V4
solve identical resulting physical states under varied topology.

## Full runnable Mac/Ubuntu experiment

From the repository root on research/kyoto-decision-coverage:

    git pull --ff-only origin research/kyoto-decision-coverage
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_export ccp_material_replay ccp_kyoto_heldout_fixture_tests ccp_kyoto_coupled_variation_tests ccp_kyoto_heldout_coupled
    build/kyoto-final-cpu/ccp_kyoto_heldout_fixture_tests
    build/kyoto-final-cpu/ccp_kyoto_coupled_variation_tests

Run the full **new-geometry** matrix with a NEW directory; this
creates three independently topologized/perturbed fixtures for
each of cloth, sponge, mixed at requested budgets 128, 512 and
1024, with both baseline and rapid regimes (54 material trajectories
in total). The 60-FRAME MATERIAL HORIZON is essential: frozen V4 can
enter a trial around frame 32 and requires up to eight SAP trial
frames before promotion. A 36-frame run is too short to judge
stable SAP selection after that transition. The original 19 November
Mac 36-frame experiment is immutable historical evidence; NEVER
overwrite its output directory. The current runner's new default
is 60 material frames; specify --frames 60 explicitly for auditability.

This study does NOT reuse original Mac Phase 18 snapshots. The
coupled audit separately runs free,
pull and fold with two cloth sheets and two spheres:

    python3 scripts/run_kyoto_heldout_research.py --exporter build/kyoto-final-cpu/ccp_material_export --replayer build/kyoto-final-cpu/ccp_material_replay --coupled build/kyoto-final-cpu/ccp_kyoto_heldout_coupled --output output/kyoto-heldout-mac-60f-new --frames 60 --warmup 8

    python3 scripts/test_kyoto_heldout_research.py output/kyoto-heldout-mac-60f-new

    open output/kyoto-heldout-mac-60f-new/HELDOUT_RESULTS.md

    python3 scripts/analyze_kyoto_heldout_evidence.py --results output/kyoto-heldout-mac-60f-new --output output/kyoto-heldout-mac-60f-new/full-method-scores --warmup 8
    open output/kyoto-heldout-mac-60f-new/full-method-scores/HELDOUT_ALL_METHODS.md

For the separately verified six-family rapid-motion matrix already
run under Linux Actions, explicitly request six families:

    python3 scripts/run_kyoto_heldout_research.py --exporter build/kyoto-final-cpu/ccp_material_export --replayer build/kyoto-final-cpu/ccp_material_replay --coupled build/kyoto-final-cpu/ccp_kyoto_heldout_coupled --output output/kyoto-six-family-mac-first --materials cloth sponge cushion soft_ball spring mixed --regimes rapid --budgets 128 512 1024 --heldout-count 3 --frames 60 --warmup 8 --coupled-frames 60 --coupled-scenarios free pull fold

The published Mac 54-scene console from 19 November 2026 had 36
material frames and 60 coupled frames. Its subsequently supplied
11-method report recorded 1895 Guard frames and 49 SAP frames,
42 of which were TRIAL frames and only seven nontrial SAP frames
(all in sponge/rapid/N1331). All 12 large rapid-cloth SAP frames
were unfinished trials at the truncated 36-frame horizon.
Corrected full 11-method ranking and caveats:
[Mac source report](KYOTO_HELDOUT_54_SCENE_MAC_FIRST_2026-11-19.md).
The original Linux six-family run had 60 material frames.
Do not compare their controller adoption fractions as
a matched temporal test.


The all-algorithm analysis is POSTPROCESSING only, reads existing
original source SHA-256 receipts, verifies strict SAP candidate count
and pair-hash equality and exposes the actual Guard/SAP/trial/switch
trajectory counts. The ten native CPU policies in the Linux suite
must never be advertised as eleven: trained ML weights and external
Bullet DBVT were not executed there. Native query timing takes place
in the research runner, not the postprocessor.

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

The existing mesh renderer accepts the new per-policy JSONL directly.
Choose one of the actual seeded solver directories, rather than a
nonexistent single shared fold path:

    python3 scripts/render_kyoto_visuals.py solver --input-dir output/kyoto-heldout-original-01/coupled/fold-geom73013-topo86011 --output output/kyoto-heldout-original-01/coupled/fold-geom73013-visual --fps 9 --stride 1

Repeat with other coupled/fold-geom*-topo* and pull/free directories
for different real mesh connectivities. Requires Pillow; ffmpeg is
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

The coupled solver diagnostics now enforce that EVERY independent
seeded geometry/topology variant produces at least one real precise
contact test and contact response, somewhere across its scenarios.
Earlier eight-frame Linux smoke evidence had zero real narrow-phase
contacts and was correctly insufficient as collision physics coverage.
That weakness motivated a longer CI contact gate. No-contact-only
solver geometry parity is not promoted as proof of collision handling.

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


## Observed first Linux native run, not a claimed Mac speedup

After strengthening the test beyond an eight-frame no-contact smoke
sequence, Linux Actions run 44465880693 passed 48-frame independent
physical solver cases with two truly different mesh triangulations,
vertex geometries and four-versus-six pinned vertex constraints.

Actual precise collision tests and sphere-contact responses occurred
in the thousands across real solver subiterations. All three original
candidate policies produced identical physical trajectories and
contact-response totals relative to SAP. However: zero **nonlocal
cloth self-contact responses** occurred, including the grab-fold
motion; and frozen V4 selected Guard for **240/240 solver queries
per scenario**, with zero SAP trials/switches. This is an important
negative experimental finding, not a successful online-switching demo.
See [the source-backed CI report](KYOTO_HELDOUT_FIRST_NATIVE_RESULT_2026-11-19.md).

The held-out Python runner now defaults to 60 coupled solver frames,
instead of the original short 24-frame exploratory setting.
An explicit fail-closed gate requires actual narrow-phase tests and
at least one contact response for each independent mesh seed over
its selected scenarios. It also records chosen Guard/SAP/trial/switch
counts from each REAL V4 solver iteration; zero activations are
preserved, not reinterpreted as speed improvements.

The full Mac experiment remains a new execution, not a rerun of the
old immutable Phase 18 scene set. No new Mac results are asserted until
the user executes it. All original threshold constants are unchanged.

## Broader Linux six-family result with negative controls

The independently verified
[54-scene ten-method source report](KYOTO_HELDOUT_54_SCENE_NATIVE_RESULT_2026-11-19.md)
is an ADDITIVE result from a single Linux CI execution
[run 44468375567](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44468375567),
not an already run Mac result. Across 54 truly seeded rapid-motion
source scenes, each with 60 frames and ten policies, all 32,400
frame/method candidate pairs matched the same-frame SAP count/hash
and 144 source-file SHA-256 receipts were independently checked.
Overall Guard/V4 geometric mean was 1.0552x with 25/54 scene
wins; selected larger cloth, ball and spring settings were better,
while cushion, sponge and mixed contained many ties or regressions.

The original coupled solver's three distinct seeded physical
geometries, each under free, pull and fold for 60 frames, accumulated
actual sphere-contact responses but ZERO naturally driven
nonlocal cloth self-contact events. Frozen V4 never selected SAP
in the 2700 coupled solver iterations. This is discrete solver
correctness and source-state parity evidence, not a successful
adaptive physical solver speedup. In a SEPARATE intentionally
overlapped patch diagnostic, self-contact correction counts were
283, 276 and 317, with disabled-collision controls at zero.

No conclusive results are claimed for independent forcing histories:
all six-family AABB replay cells use the same rapid motion class.
The original physical mesh solver still uses 13x13 grid-family
connectivity, albeit varied triangle diagonals, vertex perturbations,
spring connections and pin layouts.
