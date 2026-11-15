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

