# Kyoto Collision Broad-Phase — Research Development Log

> **Immutable evidence principle:** A speedup is a claim only after comparing exact current-AABB candidate sets, identical sequences, equivalent timed work, multiple independent seeds, warm-up treatment, confidence intervals and held-out validation. No negative results may be removed from this log. The source branch is `research/kyoto-scale-evidence`; `main` is intentionally unchanged pending review.

This is a **public experiment-and-decision log**, not private scratchpad reasoning. Every important engineering change records its purpose, observed outcome, supporting data location and unresolved limitations. Full per-frame evidence belongs in append-only, timestamped `output/` bundles; compact summaries and decisions are recorded here.

## 2026-11-15 — Baseline experiment and Mac experimental infrastructure

**Goal.** Falsify or support the hypothesis that contact-work-informed online partitioning can outperform global guarded reuse under heterogeneous deformation and larger interaction workloads.

**Existing observation.** The original 1–3 cloth coupled solver's `OnlineContactAware` method previously lost to `GlobalGuard` on the smaller November 2 workloads. CPU-multithreaded SAP measurements at 16k AABBs showed a crossover in favor of multicore execution but **do not measure the online method**. No claim of online superiority was made from SAP-only throughput.

**Implementation.** Added `ccp_material_benchmark` (cloth, sponge, cushion, soft ball, spring, mixed), `ccp_material_export` deterministic Float32 snapshot ABI, `ccp_material_replay` for identical CPU baselines and optional actual Bullet DBVT, Metal full-pair count/compaction including regional variants, `SelectiveGuard` localized repair, a generalized `GenericOnline`, separate original cloth physics simulator checks, Mac SDK diagnostics, strict pair hashes, and evidence bundles.

**Evidence.** Mac user terminal run on 2026-11-15: both smoke and standard runs finished after correction of Anaconda-shadowed CMake and SDK path; five selected CTests passed; 64-frame identical material CPU/Metal replay reported matching pair hashes; actual original coupled 3-cloth/5-sphere run reported approximate total step means: global guard 6.160 ms, online contact-work 6.785 ms, SAP rebuild 7.569 ms. These reported decimal figures are **exploratory terminal-log values**; final paired statistical analysis requires uploaded raw ZIP `output/kyoto-materials-20261115T184806Z-standard.zip`, not merely console output.

**Interpretation.** Global caching can win on low-motion/cushion scenes because per-patch maintenance exceeds avoided collision work; flat or hierarchical regions can win at higher densities. The small demo is not representative of asymptotic scaling. All new material fixtures approximate mass-spring deformation, not calibrated continuum mechanics.

**Limitation.** The original cloth-specific controller, generalized material online controller and Metal region-span selector are not identical policies. The CUDA/Metal kernel-only time is not comparable with a full CPU solver step. Pair-set equality is not a proof of physically accurate collision response.

## 2026-11-16 — PyTorch learned safe-policy selection experiment (implementation)

**Hypothesis.** An extremely small cost predictor using causal per-frame workload statistics can outperform a fixed rule on heterogeneous and held-out scenes by choosing between certified SAP, global guarded reuse, local selective guard repair and hierarchical regional search. A model trained only on independent-policy times may be overly optimistic about switching because dormant caches become stale. Therefore we need **actual closed-loop replay**, including switching, feature-extraction overhead, and cold destination cache rebuilds, before any win can be claimed.

**Algorithm design.** Preserve geometry-driven correctness: the model outputs predicted costs only and never screens individual collision pairs. The native C++ inference path accepts an explicit versioned `KYOTO_MLP_V1` artifact, 12 causal features, 0–2 ReLU hidden layers, and four cost heads. It selects actions only after a minimum dwell and a predicted saving exceeding hysteresis plus switching penalty. Destination caches are reset upon switch. Every frame's exact stable-ID pair hash is verified against the independent SAP baseline.

**Training design.** PyTorch `torch.nn.Linear` networks, smoothed-Huber loss on log measured milliseconds, AdamW, gradient clipping, validation early stopping, deterministic grid/random/exhaustive options, optional MPS acceleration, immutable train/validation/test splits of **entire independent seeds** by default, train-only feature statistics, final refit to the validation-chosen epoch, and a once-only held-out evaluation. Every trial writes `trials.jsonl` with hyperparameters, best epoch, validation loss and elapsed time. The exported model is also saved as a PyTorch state dict, with data hashes, code revision, environment metadata and complete held-out uncertainty documentation.

**Important caveat.** The initial targets are *counterfactual per-frame costs from independent warmed caches*; they do not capture the long-horizon reward of switching. A supervised MLP is a scientifically useful baseline, **not yet reinforcement learning**. Compare to simple linear and fixed-cost policies before escalating to contextual bandit or RL. Never use the test split to choose hyperparameters. GPU learned selection is a separate follow-up and should not be conflated with Metal heuristic region adaptation.

**Status at initial entry.** Implementation and correctness testing underway; NO training scores, speedup, overfitting boundary, generalization conclusion or upper scaling victory is claimed without executable experimental receipts.

**Experiment decision rule.** Promote a learned model only if (a) all pair oracles pass, (b) held-out real CPU material replay including feature and switch overhead improves median/P95 over the strongest relevant baseline across multiple independent seeds, (c) the original physics simulation is separately validated, and (d) failures/OOM/timeouts are reported symmetrically rather than treating competitors' resource exhaustion as an online speedup.

## Experiment templates for future append-only entries

Each completed run must record: UTC timestamp; Git SHA; OS/CPU/GPU/RAM; hardware thermal/clock/power state where available; material, regime, seed and primitive count; independent trial IDs; source inputs and SHA256; compared algorithms and exact policy semantics; training/validation/test split IDs; model hyperparameters, best epoch, train-vs-validation curves, prediction overhead; total wall and kernel timing boundaries; exact pair and physical correctness audits; memory peaks and timeouts; per-material median, P95 and confidence intervals; negative findings; the decision made; full raw evidence ZIP location.

Do not use training loss, oracle lower-bound action selection, or an unseen GPU kernel speedup as a substitute for a measured end-to-end deployed online policy.

## 2026-11-16 — First live PyTorch CI failures and repairs

**Observed, reproducible failure:** GitHub Actions run [43752477908](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43752477908) on Linux compiled the learned C++ policy but the new material standalone-policy stress command failed to compile: an unqualified `Policy` in global `main` was ambiguous after importing the research namespace. Fixed by validating the policy CLI against an explicit string table; no collision algorithm semantics changed.

**Observed, reproducible ML failure:** The same run successfully generated the independent teacher CSVs and installed CPU PyTorch, but the first MLP training failed with `RuntimeError: mat1 and mat2 must have the same dtype, but got Double and Float`. NumPy promoted float32 features back to float64 when applying float64 train-set normalization. Fixed by explicitly casting normalized features to `np.float32` before tensor construction. The original failed trial is **not** a performance result; the workflow artifacts contain partial raw evidence.

**Decision:** Keep a blocking CI training-and-exact-C++-replay job on each research push. Do not recommend the full Mac training sweep until Linux Python training, native C++ learned ABI/oracles, and actual held-out replay have independently passed. Capture every subsequent failure here.


## 2026-11-16 — First complete hosted PyTorch training + native learned deployment: NEGATIVE RESULT

**Verifiable source:** GitHub Actions [run 43752727859](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43752727859), artifact `kyoto-pytorch-learned-cpu-smoke` (ID 14177305527). Linux CPU, PyTorch `2.15.0+cpu`, NumPy `2.4.6`, branch commit `6fd4a51ad312c5a8f2694ce1c193760bd172b3fe`. All C++ learned artifact tests and exact full pair-hash replay checks passed. Data: 5 independent phase-offset seeds × 6 material scenes, 36 frames, 128 requested elements, baseline regime. Train seeds 211/307/503, validation seed 401, held-out seed 101. See `manifest.json` and `trials.jsonl` for all measurements; no future information used by the MLP.

**Actual hyperparameter search:** 8 seeded grid configurations, 45-epoch per-trial budget, early stopping patience 9. Best validation SmoothL1-on-log-ms score `0.2696033`; best epoch 43; selected `2×32` ReLU MLP, learning rate 0.001, weight decay 0.001, dropout 0, batch 64. Validation-selected architecture refitted on train+val at fixed 43 epochs. Test proxy top-1 cheapest-strategy classification accuracy `0.84259`; counterfactual test oracle lower bound 0.02183 ms versus predicted-chosen teacher mean 0.03227 ms (not actual ML runtime).

**Held-out ACTUAL native closed-loop execution (28 measured frames per material after 8 warm-up):** all 216 learned frames across six scenes selected **Global Guard**; learned switches = 0. Yet overhead makes the model slower than plain Global Guard on **all six scenes**. Mean ML vs Global Guard (ms): cloth 0.01784 vs 0.01087; cushion 0.01133 vs 0.00628; mixed 0.06048 vs 0.04149; soft ball 0.07486 vs 0.05299; sponge 0.03636 vs 0.02674; spring 0.05250 vs 0.04430. Geometric mean speedup `0.683×` (trajectory-resampled bootstrap 95% interval `[0.612,0.759]×`; only six independent scene types on one held-out seed). **No win is claimed**. Test correctness hash and count passed every frame. This is small-scale Linux query time, not Apple Silicon, GPU or full contact solver.

**Diagnosis supported by receipts:** the learned model never changes backend, so the neural controller cannot improve the selected collision method on this dataset; it only adds decision, feature and inference overhead. Exact separate breakdown was not present in the initial run; new `ml_feature_ms`/`ml_inference_ms`/`ml_backend_ms` telemetry has been added for the next experiment. The current dataset is too narrow for claims about high-element futureproof scaling.

**Next experimental decision:** add an event-triggered/low-cost bypass for scenes where neural inference cannot plausibly amortize; independently measure model overhead; expand training to materially larger and more diverse deformation regimes; use **new untouched test seeds** for all changes motivated by seed 101 results. Compare on actual complete `MultiCloth` physical trajectories before integrating learned selection as a default research policy. A losing model must remain preserved, not silently replaced.


## 2026-11-16 — Strict second holdout regression exposed malformed telemetry CSV

**CI failure:** [Kyoto scaling workflow 43753335669](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43753335669) passed native compilation but the learned C++ regression failed with Python `KeyError: 'ml_bypassed'`. Cause: while adding five ML telemetry columns to each CSV row, the header was not updated to include the matching columns. This was a schema error, not a geometry/collision failure.

**Repair:** Explicitly align the header with `ml_bypassed,ml_feature_ms,ml_inference_ms,ml_decision_ms,ml_backend_ms`, and strengthen the C++ regression and PyTorch loader to reject extra/unnamed CSV cells rather than training on shifted feature indices. The failed branch revision and workflow are retained for audit. **No second-run performance values** are reported from this failed revision.

**Holdout integrity:** After inspecting the initial seed-101 loss, a model-bypass path for small primitive counts was introduced. Any apparent performance improvement on seed 101 is therefore **test-contaminated** and may not count as validation. The new workflow intentionally uses independent seeds 607–1301 and tests both 128 and 512 requested elements; the ML engine must execute above the 384-AABB bypass cutoff. The bypass action is explicitly counted, not misreported as neural inference.


## 2026-11-16 — New-seed, mixed-size, deployed learned controller: second NEGATIVE overall result

**Real CI source:** [run 43753607043](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43753607043), artifact `kyoto-pytorch-learned-cpu-smoke` ID `14176934293` (downloadable from Actions). PyTorch `2.15.0+cpu`. Independently seeded trajectories not used in the first seed-101 test: train seeds 1103/1301/701/907; validation seeds 1201/607; held-out seeds **1009/809**. Six material families, baseline regime, 128- and 512-element budgets, 36 frames, 8 CPU policies (with Bullet absent in this hosted runner), and exact full collision-pair-set hash verification per frame.

**Actual hyperparameter selection:** 12 sampled grid configurations from the full 156; winning validation model 2×32 ReLU layers, lr 0.001, AdamW weight decay 0.001, dropout 0, batch 64; best validation Huber `0.1873261` at epoch 86. Test proxy top-1 cheapest-independent-cache cost classification `82.523%`; counterfactual chosen-policy mean `0.12948 ms` versus hindsight-oracle `0.08559 ms`. These proxy figures are not deployed speedups.

**Actual held-out C++ execution (30 frames after excluding first six per scene):** 24 material/seed/size trajectories; controller exact pairs matched reference for all frames. Geometric mean ML speedup against Global Guard **`0.797×`** with trajectory-bootstrap 95% interval `[0.671,0.954]×` (bootstrap groups by trajectory, not fully independent across material/seed). 3 scene trajectories faster, 21 slower. No statistical or general superiority claim. All 128-budget scenes bypass neural inference as explicitly flagged; 512-budget runs execute native neural inference (usually around 0.001–0.002 ms median) and causal feature generation (around 0.008–0.016 ms median), in addition to actual backend query.

**Promising but NOT a unique algorithm win:** For 512-target helical spring, learned chose **SAP** on every frame, beating Global Guard: 0.33083 vs 0.70622 ms on test seed 1009 (2.135×), 0.34092 vs 0.80946 ms on seed 809 (2.374×). However, the unchanged standalone **SAP** itself measured 0.32970 and 0.32788 ms, respectively; thus neural selection does **not** beat the best deployable static SAP on spring, and any narrative saying otherwise is false. It demonstrates geometry-dependent policy ranking, not a new collision algorithm advantage.

**Observed counterexample:** With 512 requested cushion elements on heldout seed 809, learned 0.12769 ms vs Global Guard 0.03351 ms (0.262×). The model chose SAP for 12 frames and guard for 24, incurring cache switch/rebuild and extra feature overhead. A learned model trained to predict separately warm independent-cache costs can make poor online switching decisions.

**Conclusion:** The precise time-breakdown instrumentation is now available; model overhead, wrong action choices, cache reset penalties and limited data diversity are primary research targets. This run is a **falsifying/negative evaluation** for promoting the current neural policy. Immediate next experiment: use more independent seeds, stress regimes, larger counts, and a switching-aware objective/trajectory cost model. Do not tune hyperparameters on these held-out seed results and then reuse seeds 809/1009 as untainted test sets.

