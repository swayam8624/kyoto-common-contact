# Verified 3D broad-phase comparison

**Important:** this is a comparison against the **actual upstream Bullet Physics btDbvtBroadphase**.
It is a serious, published-family 3D BVH baseline, **not** a claim to have beaten the world's best broad phase.
All approaches receive the same sampled 3D bounds and bilateral category/mask filtering.
The exhaustive AABB result is the correctness oracle. Every tested method must include all its required pairs.
Bullet and guarded SAP may produce conservative extra pairs; that affects follow-up narrow-phase workload.
Generated scene sheets are moving geometric AABB fixtures—not physically simulated coupled cloth.
Each algorithm includes its own construction and candidate-list materialization in the measured duration.
Oracle generation and correctness checking occur outside the timed window.

| Scenario | Primitives | Method | Mean broad (ms) | p95 (ms) | Mean candidates | Mean extra candidates | Builds | Cache reuses | Bullet updates | Misses |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| sparse256 | 256 | exhaustive_aabb | 0.086121 | 0.087375 | 0.1 | 0.0 | 32 | 0 | 0 | 0 |
| sparse256 | 256 | sweep_prune | 0.014982 | 0.015958 | 0.1 | 0.0 | 32 | 0 | 0 | 0 |
| sparse256 | 256 | bullet_dbvt | 0.204536 | 0.273500 | 0.1 | 0.0 | 32 | 0 | 0 | 0 |
| sparse256 | 256 | bullet_dbvt_dynamic | 0.088811 | 0.093417 | 0.1 | 0.0 | 1 | 0 | 31 | 0 |
| sparse256 | 256 | guarded_sap | 0.019813 | 0.022084 | 0.2 | 0.1 | 32 | 0 | 0 | 0 |
| sparse256 | 256 | region_fixed | 0.152216 | 0.174417 | 0.1 | 0.0 | 4352 | 0 | 0 | 0 |
| sparse256 | 256 | region_adaptive | 0.149714 | 0.155959 | 0.1 | 0.0 | 4352 | 0 | 0 | 0 |
| sparse256 | 256 | region_sweep_fixed | 0.162618 | 0.184250 | 0.1 | 0.0 | 4352 | 0 | 0 | 0 |
| sparse256 | 256 | region_sweep_adaptive | 0.164117 | 0.184916 | 0.1 | 0.0 | 4352 | 0 | 0 | 0 |
| clustered512 | 512 | exhaustive_aabb | 0.612605 | 0.699625 | 377.1 | 0.0 | 32 | 0 | 0 | 0 |
| clustered512 | 512 | sweep_prune | 0.175374 | 0.222250 | 377.1 | 0.0 | 32 | 0 | 0 | 0 |
| clustered512 | 512 | bullet_dbvt | 0.504615 | 0.614458 | 377.1 | 0.0 | 32 | 0 | 0 | 0 |
| clustered512 | 512 | bullet_dbvt_dynamic | 0.565099 | 1.128000 | 4871.3 | 4494.3 | 1 | 0 | 31 | 0 |
| clustered512 | 512 | guarded_sap | 0.253453 | 0.277916 | 897.4 | 520.4 | 32 | 0 | 0 | 0 |
| clustered512 | 512 | region_fixed | 0.960920 | 1.194708 | 377.1 | 0.0 | 16896 | 0 | 0 | 0 |
| clustered512 | 512 | region_adaptive | 0.945957 | 1.042666 | 377.1 | 0.0 | 16896 | 0 | 0 | 0 |
| clustered512 | 512 | region_sweep_fixed | 0.977621 | 1.068959 | 377.1 | 0.0 | 16896 | 0 | 0 | 0 |
| clustered512 | 512 | region_sweep_adaptive | 0.984259 | 1.051542 | 377.1 | 0.0 | 16896 | 0 | 0 | 0 |
| dense256 | 256 | exhaustive_aabb | 0.165031 | 0.200500 | 32376.2 | 0.0 | 32 | 0 | 0 | 0 |
| dense256 | 256 | sweep_prune | 1.561267 | 1.714083 | 32376.2 | 0.0 | 32 | 0 | 0 | 0 |
| dense256 | 256 | bullet_dbvt | 7.850135 | 8.811208 | 32376.2 | 0.0 | 32 | 0 | 0 | 0 |
| dense256 | 256 | bullet_dbvt_dynamic | 4.095395 | 4.982666 | 32376.2 | 0.0 | 1 | 0 | 31 | 0 |
| dense256 | 256 | guarded_sap | 1.580185 | 1.654292 | 32640.0 | 263.8 | 32 | 0 | 0 | 0 |
| dense256 | 256 | region_fixed | 1.197412 | 2.169000 | 32376.2 | 0.0 | 4352 | 0 | 0 | 0 |
| dense256 | 256 | region_adaptive | 1.165159 | 1.152875 | 32376.2 | 0.0 | 4352 | 0 | 0 | 0 |
| dense256 | 256 | region_sweep_fixed | 1.158219 | 1.757125 | 32376.2 | 0.0 | 4352 | 0 | 0 | 0 |
| dense256 | 256 | region_sweep_adaptive | 1.139376 | 1.480042 | 32376.2 | 0.0 | 4352 | 0 | 0 | 0 |
| sheets2 | 578 | exhaustive_aabb | 0.246118 | 0.291625 | 7431.8 | 0.0 | 32 | 0 | 0 | 0 |
| sheets2 | 578 | sweep_prune | 0.320256 | 0.378541 | 7431.8 | 0.0 | 32 | 0 | 0 | 0 |
| sheets2 | 578 | bullet_dbvt | 3.726132 | 3.979834 | 7656.4 | 224.6 | 32 | 0 | 0 | 0 |
| sheets2 | 578 | bullet_dbvt_dynamic | 0.968027 | 1.077417 | 7656.4 | 224.6 | 1 | 0 | 31 | 0 |
| sheets2 | 578 | guarded_sap | 0.027384 | 0.014542 | 12480.0 | 5048.2 | 1 | 31 | 0 | 0 |
| sheets2 | 578 | region_fixed | 0.330370 | 0.601667 | 7431.8 | 0.0 | 595 | 18445 | 0 | 0 |
| sheets2 | 578 | region_adaptive | 0.315319 | 0.479625 | 7431.8 | 0.0 | 2238 | 16802 | 0 | 0 |
| sheets2 | 578 | region_sweep_fixed | 0.335376 | 0.558500 | 7431.8 | 0.0 | 261 | 8091 | 0 | 0 |
| sheets2 | 578 | region_sweep_adaptive | 0.313366 | 0.517500 | 7431.8 | 0.0 | 994 | 7303 | 0 | 0 |
| sheets3 | 866 | exhaustive_aabb | 0.532077 | 0.586791 | 13145.7 | 0.0 | 32 | 0 | 0 | 0 |
| sheets3 | 866 | sweep_prune | 0.681616 | 1.202375 | 13145.7 | 0.0 | 32 | 0 | 0 | 0 |
| sheets3 | 866 | bullet_dbvt | 8.709122 | 9.494458 | 13457.3 | 311.6 | 32 | 0 | 0 | 0 |
| sheets3 | 866 | bullet_dbvt_dynamic | 1.814038 | 2.059084 | 13457.3 | 311.6 | 1 | 0 | 31 | 0 |
| sheets3 | 866 | guarded_sap | 0.059259 | 0.017917 | 29836.0 | 16690.3 | 1 | 31 | 0 | 0 |
| sheets3 | 866 | region_fixed | 0.571494 | 0.645250 | 13145.7 | 0.0 | 1275 | 39525 | 0 | 0 |
| sheets3 | 866 | region_adaptive | 0.585960 | 0.935167 | 13145.7 | 0.0 | 4857 | 35943 | 0 | 0 |
| sheets3 | 866 | region_sweep_fixed | 0.581348 | 0.608541 | 13145.7 | 0.0 | 510 | 15810 | 0 | 0 |
| sheets3 | 866 | region_sweep_adaptive | 0.566904 | 0.870125 | 13145.7 | 0.0 | 1901 | 13225 | 0 | 0 |
| simulated_cloths3 | 866 | exhaustive_aabb | 0.828486 | 2.158250 | 7027.0 | 0.0 | 32 | 0 | 0 | 0 |
| simulated_cloths3 | 866 | sweep_prune | 0.466962 | 0.735167 | 7027.0 | 0.0 | 32 | 0 | 0 | 0 |
| simulated_cloths3 | 866 | bullet_dbvt | 5.332717 | 6.399209 | 7027.0 | 0.0 | 32 | 0 | 0 | 0 |
| simulated_cloths3 | 866 | bullet_dbvt_dynamic | 1.153583 | 1.642792 | 7027.0 | 0.0 | 1 | 0 | 31 | 0 |
| simulated_cloths3 | 866 | guarded_sap | 0.234159 | 0.997917 | 10067.8 | 3040.8 | 10 | 22 | 0 | 0 |
| simulated_cloths3 | 866 | region_fixed | 0.405669 | 0.736083 | 7027.0 | 0.0 | 15706 | 25094 | 0 | 0 |
| simulated_cloths3 | 866 | region_adaptive | 0.464409 | 0.827083 | 7027.0 | 0.0 | 15003 | 25797 | 0 | 0 |
| simulated_cloths3 | 866 | region_sweep_fixed | 0.381658 | 0.790541 | 7027.0 | 0.0 | 3412 | 4750 | 0 | 0 |
| simulated_cloths3 | 866 | region_sweep_adaptive | 0.413882 | 0.673375 | 7027.0 | 0.0 | 3966 | 4927 | 0 | 0 |

## Interpreting the results

The independent benchmark tests **spatial candidate discovery**, not contact impulses, precise triangle geometry, CCD or solver convergence.
Both two-tier region variants first sweep union-envelope AABBs and only then consult certified region-pair candidate blocks. The original all-region-pairs method remains a distinct ablation. The hierarchy is a known architecture family, NOT asserted historically novel.
The `simulated_cloths3` case records 32 actual configurations produced by the same C++ multi-cloth XPBD-style contact-response simulation. This is **not** synthetic sinusoidal geometry, and the method timings exclude the cost of producing those snapshots.
The guarded policy's candidate count can be larger than freshly regenerated pairs. Whether its saved builds improve end-to-end contact time depends on the *extra precise tests* a caller must run.
To avoid a weakened external baseline, we compare both **freshly reconstructed** Bullet DBVT and **one persistent Bullet DBVT updated on every geometry change**. Those are different computational regimes; persistent Bullet reuses its spatial tree but does not reuse a frozen pair list.
Those extra tests have NOT been modeled as invented time savings. The existing live cloth benchmark separately measures real candidate-only precise work, whole-step time, and validation.

## Literature and implementation

- Bullet Physics real DBVT baseline: https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/BroadphaseCollision/btDbvtBroadphase.h
- Erin Catto's Box2D dynamic tree design documentation (related family, 2D, not executed here): https://box2d.org/documentation/group__tree.html
- Tero Karras, *Maximizing Parallelism in the Construction of BVHs, Octrees, and k-d Trees* (HPG 2012). GPU LBVH is **not** evaluated here: https://research.nvidia.com/publication/2012-06_maximizing-parallelism-construction-bvhs-octrees-and-k-d-trees
- Cohen et al., *I-COLLIDE* (1995), historical sweep-and-prune foundation: https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/cohen95.pdf

## Novelty boundary

Neither SAP, DBVT, fat AABBs, nor temporal pair caches are novel by themselves.
The research hypothesis to test is a conservative, **method-aware, query-contract-based** reuse interface with measured full-pipeline benefits across distinct solver geometries and semantics.
That cross-simulator hypothesis is **not yet demonstrated**. Current novelty is an implementation/investigation direction, not a verified publication-level novel algorithm.
