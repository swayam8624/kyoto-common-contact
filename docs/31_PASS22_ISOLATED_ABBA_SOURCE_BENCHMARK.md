# Pass 22B — A fairer isolated A/B/B/A experiment on original 3D FEM

## Why the earlier benchmark was inconclusive

Every original per-scene benchmark in Pass 21 passes exact candidate
coverage, but its internal A/A timing controls revealed execution-order
sensitivity. The native multi-physical-source run showed:

- When **A ran first**, A/B total paid clock ratio approximately 1.024.
- When **B ran first**, A/B total paid clock ratio approximately 0.887.
- All original source frames were deterministic solver trajectories,
  yet the AB ordering effect was large enough to explain several
  of the small reported algorithm performance gaps.

The earlier five-method benchmark repeatedly ran many memory-intensive
hierarchies and IPC pipelines in one fixed frame order. The two A/A
instances were executed after the main methods. Even if the nominal
order rotated, the memory-cache and allocation state presented to each
method was not equivalent. A 1–3% aggregate advantage is not proof of
a better broadphase under such a protocol.

**Never claim that a reported IPC/selective ratio of 1.02 from that
experiment is statistically significant.** It is a workload-specific
timing observation confounded by order/cache effects.

## Isolated ABBA design

The original C++23
`apps/pass15_official_ipc_fem_benchmark.cpp` now accepts an optional
last argument `abba`. It uses exactly the same original physical
source generator and pinned IPC Toolkit LBVH, but bypasses the long
multi-variant benchmark for this isolated timing trial.

Four INDEPENDENT physically advancing broadphase instances are used:

- A1: maintained selective feature-box/ancestor BVH.
- A2: a second independently maintained identical selective BVH.
- B1: official pinned IPC LBVH rebuilt each source step, with a
  separately maintained cached-input geometry/connectivity adapter.
- B2: a second independent IPC LBVH and input adapter.

**All four instances advance exactly once per original physical frame.**

This is critical: calling selective `query` twice on one instance
would make the second call falsely appear faster because its motion
cache had already consumed the new frame. A1 and A2 instead each
maintain their own previous original source state, just as B1/B2 do.

The paid timed scope starts immediately before the selected C++ query
and ends after it returns the normalized candidate set. Actual
source FEM integration, independent exhaustive candidate generation
and CSV writing remain outside the measured competing methods.

For each frame/trial, the execution order alternates among

```text
A1 — B1 — B2 — A2
A2 — B2 — B1 — A1
B1 — A1 — A2 — B2
B2 — A2 — A1 — B1
```

to counterbalance both **which method is outermost** and
**which independently allocated instance runs earlier**.

Every original result from A1, A2, B1 and B2 must independently
match the exhaustive VF/EE reference candidate set. Output includes
the source physical scene, original time, original contact updates,
four exact wall-clock costs, four candidate counts, cold-frame flag,
repetition and actual execution order.

## Source cohort and protocol

Cohort 1 uses `scene_count=4`, `sample_stride=12`, four repeated
trials, covering original 12/20/24-major neo-Hookean meshes, a
separately accepted high-speed impact and TWO independently
time-integrated FEM bodies sharing one finite static patch.

Cohort 2 uses `scene_count=1`, `sample_stride=1`, four repeats,
covering EVERY one of the 360 routine accepted physical steps plus
the original high-speed accepted adaptive leaves.

The script `scripts/report_pass22_abba.py` validates every raw
native field, confirms complete candidate coverage, checks exactly
one cold source frame per scene and repetition, rejects duplicate or
missing physical intervals, then separates

1. cold topology epochs from warm repeated source traversal;
2. A-outer and B-outer measurement order;
3. independent A1/A2 and B1/B2 same-algorithm clock ratios;
4. per-physical-scene, per-trial, and overall total paid ratios;
5. the specific uncoupled physical two-body workload.

The key quantity, **official cached-input IPC / selective refit**,
is greater than 1 only when selective is measured faster.

If results disagree materially between A-outer and B-outer orders,
we must record the difference and avoid strong speedup claims.
A difference across correlated time frames does not supply
independent statistical significance; independent hardware,
physical solvers and materials are still needed.

## Run

```bash
cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
  -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
cmake --build build-ipc --parallel 4 \
  --target kyoto_pass15_official_ipc_fem_benchmark
python3 -m unittest discover -s tests \
  -p 'test_pass22_abba_receipts.py' -v
./build-ipc/kyoto_pass15_official_ipc_fem_benchmark \
  results/kyoto-abba-multi 12 4 4 abba
python3 scripts/report_pass22_abba.py \
  --input results/kyoto-abba-multi/official_ipc_isolated_abba.csv \
  --output results/kyoto-abba-multi/science
```

For every solver frame, `official_ipc_isolated_abba.csv` is ORIGINAL
unmodified C++ timing evidence. Python is only validation, arithmetic,
plotting and interpretation. The graphs are descriptive.

## Remaining limitations

An ABBA protocol helps expose and counterbalance cache/order effects,
but cannot make branch-prediction, allocator, thermals or core scheduling
identical between two fundamentally different data structures.
No end-to-end physical solver speedup is measured; all source dynamics
are integrated outside the competing broadphase clocks. A true
publication-level claim still needs independent event-level CCD,
a second solver and matched baselines beyond pinned IPC Toolkit LBVH.
