# Patch-size calibration and later-frame evaluation

The C++ experiment profiles 12 variants: six mesh-region cell spans (1, 2, 3, 4, 6, 12), both the original flat patch-pair enumeration and the new hierarchical envelope sweep.

Calibration and evaluation use DISJOINT later and earlier time windows of one deterministic three-cloth physics trajectory. The selected policy is chosen using calibration complete-step time ONLY. Evaluation is reported on later frames that do not take part in the selection.

Every alternative must preserve all physical particle positions against full sweep-and-prune (maximum error below 1e-8); conservative broad-phase query coverage also has its own independent oracle tests.

| Region span | Frontier | Time window | Full step (ms) | Broad phase (ms) | Precise response (ms) | Candidates | Coarse pairs skipped | Region builds | Region reuses |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | sap_rebuild | calibration | 3.495 | 2.548 | 0.607 | 7073.300 | 0.000 | 0.000 | 0.000 |
| 0 | sap_rebuild | evaluation | 11.421 | 4.658 | 6.404 | 13265.400 | 0.000 | 0.000 | 0.000 |
| 1 | all_region_pairs | calibration | 28.387 | 27.350 | 0.606 | 7073.300 | 0.000 | 40957.750 | 431017.250 |
| 1 | all_region_pairs | evaluation | 36.888 | 29.929 | 6.454 | 13265.400 | 0.000 | 43970.800 | 428004.200 |
| 1 | two_tier | calibration | 4.500 | 3.523 | 0.570 | 7073.300 | 91246.400 | 1345.900 | 14390.900 |
| 1 | two_tier | evaluation | 14.990 | 8.092 | 6.414 | 13265.400 | 87195.000 | 2929.600 | 33035.700 |
| 2 | all_region_pairs | calibration | 3.451 | 2.522 | 0.551 | 7073.300 | 0.000 | 2783.400 | 27741.600 |
| 2 | all_region_pairs | evaluation | 11.089 | 4.377 | 6.267 | 13265.400 | 0.000 | 3641.850 | 26883.150 |
| 2 | two_tier | calibration | 2.644 | 1.744 | 0.531 | 7073.300 | 5394.650 | 327.000 | 3221.300 |
| 2 | two_tier | evaluation | 10.586 | 3.851 | 6.289 | 13265.400 | 4893.450 | 693.450 | 5358.450 |
| 3 | all_region_pairs | calibration | 2.519 | 1.602 | 0.545 | 7073.300 | 0.000 | 615.250 | 5759.750 |
| 3 | all_region_pairs | evaluation | 10.100 | 3.409 | 6.253 | 13265.400 | 0.000 | 958.250 | 5416.750 |
| 3 | two_tier | calibration | 2.356 | 1.473 | 0.523 | 7073.300 | 944.600 | 162.050 | 1488.550 |
| 3 | two_tier | evaluation | 10.329 | 3.480 | 6.370 | 13265.400 | 770.550 | 397.850 | 2122.650 |
| 4 | all_region_pairs | calibration | 2.332 | 1.445 | 0.526 | 7073.300 | 0.000 | 246.500 | 1928.500 |
| 4 | all_region_pairs | evaluation | 10.342 | 3.573 | 6.329 | 13265.400 | 0.000 | 451.050 | 1723.950 |
| 4 | two_tier | calibration | 2.323 | 1.440 | 0.526 | 7073.300 | 250.850 | 108.950 | 807.800 |
| 4 | two_tier | evaluation | 10.335 | 3.587 | 6.304 | 13265.400 | 172.900 | 302.400 | 1006.950 |
| 6 | all_region_pairs | calibration | 2.431 | 1.547 | 0.523 | 7073.300 | 0.000 | 61.200 | 463.800 |
| 6 | all_region_pairs | evaluation | 10.708 | 3.970 | 6.303 | 13265.400 | 0.000 | 141.350 | 383.650 |
| 6 | two_tier | calibration | 2.519 | 1.606 | 0.541 | 7073.300 | 30.700 | 46.600 | 324.900 |
| 6 | two_tier | evaluation | 10.854 | 4.060 | 6.350 | 13265.400 | 2.250 | 139.850 | 373.900 |
| 12 | all_region_pairs | calibration | 2.314 | 1.428 | 0.528 | 7073.300 | 0.000 | 7.950 | 67.050 |
| 12 | all_region_pairs | evaluation | 11.228 | 4.507 | 6.285 | 13265.400 | 0.000 | 23.250 | 51.750 |
| 12 | two_tier | calibration | 2.294 | 1.424 | 0.513 | 7073.300 | 4.400 | 6.350 | 46.650 |
| 12 | two_tier | evaluation | 11.213 | 4.493 | 6.286 | 13265.400 | 0.250 | 23.050 | 50.500 |

## Training-time choice and untouched later-frame result

Calibration chose **two_tier**, patch span **12** cells.
Later-frame evaluation: **11.2134 ms**, SAP **11.4213 ms**, original flat span-three **10.1001 ms**.
Evaluation step-time reduction relative to SAP: **+1.82%** (negative = slower).

## Limitations

This is STATIC profile-guided granularity, not a learned online mesh partitioner. One deterministic scene and one timing run cannot establish generality; repeat with other trajectories and hardware before making speedup claims.

AABB, region pruning, temporal caches, hierarchical spatial search and profile-guided tuning have significant prior art. This study tests their interaction and correctness but does not prove a previously unpublished algorithm.