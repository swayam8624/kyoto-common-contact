# Online broad-phase controller — verified physical time-series

**Implemented:** at every physics step, collect actual per-region AABB checks, cached-pair revalidations, candidate counts and confirmed contact corrections. A bounded local controller periodically splits contact-heavy 12×12/6×6 mesh patches into four children (minimum 3×3), merges quiet four-child quartets, and can change flat-versus-two-tier region traversal according to observed region overlap and measured broad-phase timing. Repartitioning resets candidate certificates BEFORE subsequent queries.

**Correctness boundary:** narrow-phase mathematics and XPBD-style solver are unchanged; frame-by-frame particle positions match SAP to <1e-8. Exhaustive AABB candidate oracles run on periodically audited frames. No continuous collision detection or production-level nonpenetration guarantee.

Total recorded frames: **160**. Measured average after first 12: online 12.5724 ms/step, SAP 13.7562 ms/step.
Measured percentage reduction: **+8.61%** (a negative value means the online controller was slower).
Real geometry: **12 patch splits**, **0 patch merges**, **4 front-end strategy switches**.
Audited frames: **11**.
Mean online controller decision time: **0.00498 ms**.
Mean online broad-phase service: **3.88165 ms**.
Mean SAP broad-phase service: **5.14752 ms**.

## Every observed controller decision

| Frame | Action | Patches | Contact corrections | Region AABB work | Broad phase (ms) | Complete online step (ms) |
|---:|---|---:|---:|---:|---:|---:|
| 12 | split ×1, strategy switch ×1 | 15 | 0 | 261968.0 | 1.1518 | 1.6474 |
| 24 | split ×1 | 18 | 144 | 593416.6 | 1.9172 | 3.3910 |
| 30 | strategy switch ×1 | 18 | 613 | 753946.9 | 2.7012 | 7.2435 |
| 36 | split ×1 | 21 | 1653 | 1243686.5 | 3.9473 | 10.2458 |
| 48 | split ×1, strategy switch ×1 | 24 | 4982 | 1274905.1 | 5.0800 | 15.3175 |
| 60 | split ×1 | 27 | 4702 | 906454.4 | 4.3576 | 15.1533 |
| 72 | split ×1 | 30 | 3984 | 860375.2 | 4.4611 | 16.0794 |
| 84 | split ×1 | 33 | 3910 | 879084.8 | 5.1018 | 14.4157 |
| 96 | split ×1 | 36 | 3407 | 664075.9 | 4.0449 | 13.2521 |
| 108 | split ×1 | 39 | 3002 | 598483.6 | 3.9617 | 13.0049 |
| 114 | strategy switch ×1 | 39 | 2973 | 564253.9 | 3.9645 | 13.2801 |
| 120 | split ×1 | 42 | 2933 | 524463.0 | 3.9161 | 14.5423 |
| 132 | split ×1 | 45 | 3138 | 516828.1 | 4.0200 | 13.8718 |
| 144 | split ×1 | 48 | 3084 | 515875.2 | 4.1159 | 14.0070 |

## Experimental limitations and prior art

The controller is genuinely online and changes the region partition, but its thresholds, EWMA coefficients, controller work model and hysteresis are engineering heuristics and are not proved optimal. Its strategy-switch measurements are not same-state paired A/B timings. These results are a single sequential CPU run, not randomized controlled trials or proof of generalization.

Region splitting/merging, automatic broad-phase pruning, dynamic BVH maintenance, and contact-aware cloth culling have significant prior art: PhysX ABP/PABP, Bullet DBVT, solver-integrated deforming BVH updates (TVCG 2026), and curvature-based collision culling (Mathematics 2026). This implementation alone does not establish publishable novelty or superiority.
