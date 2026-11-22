# Pass 3 external verification — official IPC Toolkit LBVH v1.6.1

**Status: EXTERNAL REFERENCE INTEGRATION PASSED for the declared tests.**
This document records an actual compiled upstream library, not a simulated
stand-in and not a performance/physics/novelty claim.

## Source and binary provenance

- Official repository: https://github.com/ipc-sim/ipc-toolkit
- Pinned stable release: v1.6.1
- Pinned source commit: 19bcab7e1f5a0d2addfcd28513422ff248ea504e
- Kyoto opt-in test: tests/ipc_lbvh_parity_tests.cpp
- CMake option: KYOTO_WITH_OFFICIAL_IPC=ON
- GitHub Actions official external verification:
  https://github.com/swayam8624/kyoto-common-contact/actions/runs/45069408049
- Source CI transcript: artifact kyoto-official-ipc-v1-6-1-build-evidence

## What was genuinely compared

The new C++23 mesh contract expresses two typed candidate families:
vertex-face (VF) and edge-edge (EE). Each original vertex uses a
conservative *cubic-control* swept AABB derived from the same vertex
motion input. Edges/faces are built using identical connectivity and
topological incidence exclusions.

The external adapter converts exactly the same original swept
vertex boxes to official ipc::AABB objects, initializes official
ipc::LBVH with the actual vertex/edge/face incidence matrices,
and retrieves its official edge-edge and face-vertex candidates.

Both methods use all-compatible collision masks, the same inflated
bounds and a 3D feature interpretation. Original ID normalization is
performed before comparison. The test requires every independent
double-precision reference pair to be present in the official
conservative candidate output. Additional official conservative pairs
would be reported, not incorrectly counted as a failure; for the
tested scenes, there were none.

No actual continuous CCD operation was run in this test. No
comparative timing benchmark was collected. The ~0.02s CTest duration
is **test harness runtime**, not a fair algorithm speed comparison.

## Verified native output

| Seed | Independent reference | Actual official IPC LBVH | Extra |
|---:|---:|---:|---:|
| 0 | 2 | 2 | 0 |
| 1 | 3078 | 3078 | 0 |
| 2 | 3086 | 3086 | 0 |
| 3 | 3086 | 3086 | 0 |
| 4 | 3022 | 3022 | 0 |
| 5 | 3080 | 3080 | 0 |
| 6 | 3072 | 3072 | 0 |
| 7 | 3058 | 3058 | 0 |
| 8 | 3058 | 3058 | 0 |
| 9 | 3073 | 3073 | 0 |
| 10 | 3072 | 3072 | 0 |
| 11 | 3048 | 3048 | 0 |
| 12 | 3064 | 3064 | 0 |
| **Total** | **36,799** | **36,799** | **0** |

All 13 tests passed. The external GitHub workflow configured,
compiled and ran the actual pinned upstream IPC Toolkit.

## Reproduce the opt-in integration

Use a CMake 3.24+ compiler environment that can fetch external dependencies:

    cmake -S . -B build-ipc \
      -DCMAKE_BUILD_TYPE=Release \
      -DKYOTO_WITH_OFFICIAL_IPC=ON \
      -DIPC_TOOLKIT_WITH_SIMD=OFF \
      -DIPC_TOOLKIT_WITH_MESHFEM_SPARSE=OFF

    cmake --build build-ipc --target kyoto_official_ipc_lbvh_tests --parallel 4

    ctest --test-dir build-ipc \
      -R pinned_official_ipc_lbvh_feature_coverage \
      --output-on-failure -V

The default normal build remains dependency-free; optional IPC source
is not copied into the Kyoto repo. The external toolkit has its own
licence and dependency obligations.

## Limitations / next scientific gate

- 13 synthetic 3D scenes only; dense all-overlap random scenes are not
  representative of low-overlap large deformable meshes.
- Feature-level candidate parity on equivalent inflated AABB inputs,
  not *general* continuous collision detection.
- No proof about numerical worst cases, all solvers, topology mutation
  during a query or GPU parity.
- No actual solver with strong self-contact and no external measured
  trajectory dataset yet.
- No official LBVH build/query timing comparison under matched
  conditions, no total narrow phase, no physics step speedup.
- No novel algorithm has been demonstrated yet; this is a reliable
  *reference implementation and input contract* needed for research.

**Next:** feed the real solver's connected deformable surfaces,
match an independently verified CCD event set, and perform a fair
paid end-to-end performance comparison against this actual upstream
LBVH. Only then profile SCA hierarchy or selective time refinement.
