# Pass 12 — Measured finite-mesh FEM energy accounting

## Purpose and frozen inputs

The first Pass 11 real 3D 900-step finite-triangle obstacle integration has
**physical source states**, true face-contact impulse receipts, a positive
tetrahedral Jacobian and a static original 901-state mesh separation audit.
Those facts prove important *finite-test* properties. They do not establish
physical calibration, accuracy against a converged independently implemented
solver, or speedup against the official IPC Toolkit.

Pass 12 introduces a separate **read-only data-auditing path**. It does not
modify the FEM integrator, contact law, original mesh, time step, or existing
900-step results.

Native source:
`./build/kyoto_rigid_mesh_fem_tests results/pass12/source`

Data:
`results/pass12/source/rigid_triangle_physics_900.csv`

## Reproduce the account

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kyoto_rigid_mesh_fem_tests --parallel 4
python3 -m unittest discover -s tests -p test_pass12_energy_audit.py -v

./build/kyoto_rigid_mesh_fem_tests results/pass12/source
python3 scripts/audit_pass12_mesh_energy.py \
  --input results/pass12/source/rigid_triangle_physics_900.csv \
  --output results/pass12/energy

python3 -m pip install matplotlib
python3 scripts/render_pass12_mesh_energy.py \
  --audit results/pass12/energy --output results/pass12/visuals
```

The new independent GitHub Actions workflow
`.github/workflows/pass12-energy-audit.yml` reproduces the native source,
executes separate synthetic accounting tests, creates the original-data
account and archives all three plots. Workflow success is not claimed until
the Actions run actually passes.

## Exact accounting identities

For original accepted macro step `n`, define its measured total mechanical
energy as `E_n = K_n + U_n + G_n`. The discrete observation is

`delta E_n = (K_n-K_(n-1)) + (U_n-U_(n-1)) + (G_n-G_(n-1))`.

The independent auditor checks the telescoping identity:

`sum(n=1..N) delta E_n = E_N-E_0`.

It checks all native state/counter columns are finite, consecutive step IDs,
strictly increasing time, positive finite-face separation and positive FEM
Jacobian. It also checks that each macro step's accepted solver duration
equals its actual physical clock advancement.

Each source step is labelled `contact` if its genuine original
`face_impulse_updates` is nonzero; otherwise `no_contact`.

**Crucial caution:** A negative discrete mechanical-energy change is not
necessarily contact friction or correct physical dissipation. Numerical
integration, explicit damping, sequential impulses and friction all
contribute; contact changes the state and hence other losses. A contact
versus no-contact plot is **correlation**, not a causal decomposition.

The old source measurement of approximately −389.745 J is kept, not
reinterpreted as physical correctness. The new scripts must obtain their
numbers from the freshly generated CSV rather than embedding this value.

## Output and science visualization

- `mechanical_energy_steps.csv` — original-source per-step changes in
  kinetic, elastic, gravity and total mechanical energy, plus accepted
  contact/subdivision receipts.
- `mechanical_energy_audit.json` — start/end energy, telescoping residual,
  actual elapsed time, minimum geometry/Jacobian, contact and non-contact
  aggregate deltas; explicit uncertainty statement.
- `12a_mechanical_energy_observed.png` — actual cumulative energy evolution.
- `12b_actual_contact_activity.png` — accepted impulse updates over real time.
- `12c_energy_contact_correlation_not_causation.png` — observed stepwise energy
  changes grouped by actual source contact, with no inferred damping force.

The entire pipeline uses genuine 3D FEM source records. Its figures do NOT
depict fabricated contact forces, and its synthetic Python unit rows never
become source evidence.

## Next required scientific gate: paid SCA versus official IPC

The central thesis remains: does a maintained, locally refitted Spacetime
Contact Atlas beat official pinned IPC Toolkit LBVH on identical held-out
genuine deforming feature trajectories?

Before measuring, freeze scene/seed split, compiler/options, broadphase
feature semantics, timestep, topology epochs, accepted/rejected solver
steps and precise timing boundaries. Run both methods over identical
serialized solver states, verify complete candidate coverage and identical
exact narrowphase CCD event outcome. Account *all* data preparation,
hierarchy construction, refit, query, invalidation, fallback and memory
allocation. Benchmark fresh starts and warm caches separately. Randomize
and counterbalance method order, include A/A timings, report per-trajectory
ratios, paired confidence intervals, P50/P95 and worst-case costs, and
report any unmatched feature pair/CCD event as a correctness failure.

No claim of general speedup or of novel collision mathematics follows
from the Pass 12 energy-analysis workflow. Until the above experiment is
implemented and executed, **the full-paid performance gate remains open**.
