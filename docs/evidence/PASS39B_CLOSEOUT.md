# Exact arithmetic optimization: Pass 39B

## Pass 39B — CLOSED: exact arithmetic optimization milestone

The previously named B2 is part of this single optimization milestone,
not a separate unfinished pass. The original determinant expansion,
factored normal, and constant-normal specialization are now compared
at coefficient, complete pair, and complete volume-audit level.

### Held-out execution

- [x] Recover six actual repository headers into the local Linux
  execution environment, verifying all six Git blob hashes against
  pinned commit 5714095a00b457b3fd8a36fe48c918c01d79a01a. Compile on GCC 14.2 and Clang 17,
  strict C++23 warnings-as-errors, -O2 -ffp-contract=off -fno-fast-math.
- [x] 480 new dyadic paths (seed 3917129, 120 per adjacency stratum,
  scales 2^-500, 1, 2^500, mixed common shear and independent
  perturbation, shuffled vertex indices). All three full pair
  receipts equal; swapped reference/specialized receipts equal.
- [x] Independent exact static intersection at nine dyadic times
  per trajectory: 4,319 nondegenerate comparisons per compiler,
  one degenerate sample excluded and explicitly counted. Zero
  contradictions of either continuous direction. 395 certified,
  85 unresolved; 78 paths with a sampled forbidden intersection.
  The remaining seven unresolved paths are not claimed collision-free.
  Sampling is a falsifier, not a complete continuous oracle.

| Shared vertices | Paths | Certified | Unknown | Sampled hit paths | Static checks |
|---:|---:|---:|---:|---:|---:|
| 0 | 120 | 118 | 2 | 1 | 1080 |
| 1 | 120 | 94 | 26 | 24 | 1080 |
| 2 | 120 | 97 | 23 | 21 | 1079 |
| 3 | 120 | 86 | 34 | 32 | 1080 |

- [x] Held-out full-volume audit: 24- and 72-tet structured boxes
  with static, shear, nonuniform and deliberately inverted paths.
  All six valid cases certified and both inverted cases refused,
  with identical complete work receipts for all three modes on both
  compilers. 9,024 exact pair-path checks across the eight distinct
  mesh trajectories (not independent physical simulations).
- [x] Paid six-slot mirrored Latin rotations, three measured blocks
  per stratum/case, one excluded warmup. Pair timing: 72 batches per
  compiler, 120 paths per batch; mesh timing: 144 audits per compiler.
  Timing includes candidate generation for the mesh audits, exact
  orientation, witnesses and verification. Equality checking follows
  each timed call. Raw rows and work-counter parity checked.

| Workload | Compiler | Reference/factored | Reference/specialized | Factored/specialized |
|---|---|---:|---:|---:|
| Held-out pairs | GCC | 1.958110 | 2.310519 | 1.179974 |
| Held-out pairs | Clang | 2.039014 | 2.327196 | 1.141334 |
| Held-out meshes | GCC | 2.121471 | 3.088246 | 1.455710 |
| Held-out meshes | Clang | 2.196383 | 3.100765 | 1.411760 |

Equal-weight geometric means of paired block sums; repeated blocks
measure repeatability, not independent scene population inference.
Shared Linux VM, no isolated hardware or production FEM timing.

### Decision and scope

- [x] Freeze the arithmetic work. Recommend explicit
  `factored=true, constant_fast=true` for the optional exact audit;
  preserve historical default and explicit original arm for
  reproducibility. No approximation or threshold change.
- [x] Register complete pair/volume parity regressions and preserve
  four raw CSV files with exact locally executed blob hashes.
- [x] Pass 39B acceptance complete at standalone exact-audit scope.
  Do not extend this milestone with additional arithmetic subpasses.
- [ ] Pass 39C: integrated native physical CPU/GPU comparisons.
  Full clone still fails DNS; queued/pending CI has not established
  native solver build or execution. External solver integration and
  manuscript remain open. No whole-FEM speedup is claimed.

Reproduce on a checkout:

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_axis_heldout_tests.cpp -o /tmp/kyoto-heldout
    /tmp/kyoto-heldout --timing > /tmp/heldout-gcc.csv
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_axis_heldout_volume_tests.cpp -o /tmp/kyoto-heldout-volume
    /tmp/kyoto-heldout-volume > /tmp/mesh-gcc.csv

Use clang++ with the same flags for the other compiler. Recorded CSVs:
`docs/evidence/pass39b-final-{heldout,mesh}-{gcc,clang}.csv`.
