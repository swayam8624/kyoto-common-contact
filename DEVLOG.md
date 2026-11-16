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


## 2026-11-16 — Independent-process stress harness CI validation (not extreme scaling proof)

**Source:** GitHub Actions [run 43754092139](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43754092139), `cpu-reference` job, `kyoto-cpu-stress-smoke` artifact. A randomized 12-execution subset of the quick stress matrix tested individual policies on deforming cloth/sponge/soft-ball scenes, requested sizes 512 and 2,048, baseline/rapid regimes, fixed seeds, 24 frames, a 60-second per-policy timeout and a uniform 1,024-MB sampled RSS cap. **12/12 isolated executions completed and passed the exact current-AABB coverage checks.** The script wrote a full independent-method manifest, per-frame CSV and logs.

**Important sampling issue:** The 100-ms peak-RSS polling interval returned zero for several ultra-short processes, so the reported 0-MB sample must never be interpreted as zero memory consumption. The stress output is a **harness correctness smoke test**, not a comparative ranking or a claim of constant-memory execution. Full 16k–100k Mac CPU physical workloads and fair timing/memory analysis remain **not executed**; hardware follow-up uses `scripts/run_kyoto_stress.py stress` and `extreme` with transparent censoring.

**Workflow policy:** After two actual PyTorch learned-controller experiments, the costly `learned-training-smoke` CI job was made opt-in through GitHub Actions `workflow_dispatch`; all native C++/Metal compilation, exact-oracle regressions, and small independent stress-smoke checks still run on research branch pushes. No neural wins are accepted merely because a CI test passes.


## 2026-11-16 — User Mac quick/standard success, exhaustive 34/156, postrun packaging failure and non-destructive resume

**Source:** user-provided Mac terminal logs `Pasted markdown(20261115-195455).md`, `Pasted text (2)(20261116-045946).txt`, `Pasted text (3)(1).txt`. The first two full material experiments ended with `[KYOTO-ML] SUCCESS`. The **quick** run completed under `output/kyoto-learned-20261115T195351Z-quick`, and the **standard** completed under `output/kyoto-learned-20261115T195639Z-standard` after collecting all 384 paired teacher trajectories and running 48 PyTorch trial configurations. Standard held-out deployed CPU learned selector: 96 test-scene trajectories (2 test seeds × 6 materials × 4 regimes × 2 budgets); reported geometric-mean speedup over Global Guard `0.917x`, trajectory-bootstrap 95% interval `[0.862x, 0.972x]`, 21 trajectories faster and 75 slower. **The learned selector still loses overall**; full original coupled physics simulation, large-scale and GPU neural comparisons remain unproven.

**Bug (confirmed):** both successful runs subsequently emitted `Exception ignored in atexit callback` and `TypeError: expected str, bytes or os.PathLike object, not int` because `run_kyoto_ml_mac.py` passed a native Python integer as the `--exit-code` element of `subprocess.run` for the evidence packager. This is a **post-success ZIP packaging failure**, not a collision or model failure. The full `output/` directories remain intact. Fixed by passing `str(...)` and supporting explicit manual packaging of completed output directories.

**Exhaustive run:** `output/kyoto-learned-20261116T050002Z-exhaustive` collected all **1,080** material snapshot/CPU teacher-replay experiments, each with 100 frames, and had reached **trial 34/156** in PyTorch validation search. The supplied log ends at trial 34 with no Python exception or traceback, so there is **no evidence of a code crash at that point**; a long-running MPS optimization trial or stalled local process remain distinct possibilities. The experiment is expensive (156 hyperparameter configurations across up to 500 epochs, after 1080 material runs). The existing `model/trials.jsonl` stores at least the completed first 34 trials unless manually modified; do not delete or overwrite these data to retry.

**Recovery engineering:** introduce `--resume-run <same-existing-output-folder>` in the Mac wrapper and `--resume` in the PyTorch trainer. Resumption skips complete immutable teacher/snapshot files and validates the existing trial indices, exact configurations and numeric validation records, reusing all finished trials and restarting only the incomplete trial. Future runs store a strict dataset/parameter search contract and print epoch-level loss and elapsed time in addition to trial summaries. The historical run had no search contract, so its data/configuration are anchored at first verified resumption with an explicit warning. Preserve the original developer log, source revision, trial JSONL, model files and held-out evidence; refuse duplicate training while a matching process is still alive.

**Partial-run evidence:** Full auto-ZIP packaging of 1080 binary snapshots on a failed/interrupted exit could look like another hang and increase disk pressure. The packager now writes a small `-partial-<UTC>.zip` containing diagnostics, trial records and logs while **leaving full teacher/snapshot CSV+binary data untouched on disk**. A successful run can create the original full ZIP; each file is checksum-registered and packaging reports progress. This separation is essential for safely resuming large exhaustive experiments.

**Scientific decision:** Keep the 0.917x standard loss as a valid negative observation, with no claim that exhaustive hyperparameter search will necessarily reverse it. Do not tune on held-out seeds after seeing these numbers and reuse them as untainted independent evaluation without relabeling the study as exploratory.


## 2026-11-16 — Verified interruption recovery in real PyTorch CI

**Reproduction:** [GitHub Actions workflow 43827512066](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43827512066) passed all three scaling jobs: Linux CPU reference/oracle tests, macOS Metal shader compilation, and a manually gated CPU PyTorch training-and-resume integration. The separate native core regression [workflow 43827512053](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43827512053) passed both Linux and macOS jobs.

**Saved-trial replay test:** The CI job trained 12 real hyperparameter configurations, then constructed a representative *legacy* interrupted model directory containing the first two `trials.jsonl` rows, no model file and no modern `search_contract.json`. Running the trainer with `--resume --max-trials 3` validated the retained trial indices/configurations, printed the explicit legacy warning, computed only trial 3, successfully exported a native C++ model and validated that the resulting trial history was **exactly [0,1,2]**. All original teacher CSVs were untouched. This verifies recovery semantics using actual PyTorch and real collision-data teacher trajectories; the user's specific 34/156 Mac run was not independently executable on CI because its locally generated dataset was not uploaded.

**Packaging/append-only test:** `scripts/test_ml_recovery.py` additionally passed in Linux CI. It proved an interrupted diagnostic ZIP omits heavy original snapshots/teacher CSV while preserving the originals, completed full archives include those data, existing archives cannot be overwritten, and re-entered subprocess logs append rather than erase previous trial records.

**Decision:** New Mac CLI `python scripts/run_kyoto_ml_mac.py exhaustive --resume-run output/kyoto-learned-20261116T050002Z-exhaustive` is now released for local use **only after checking that the prior train process has fully stopped**. It resumes from the count actually present in the local trials file, validates the deterministic original hyperparameter grid, and emits epoch-level progress. Do not delete the 1,080 existing scenes. The expensive routine PyTorch CI job is restored to explicit `workflow_dispatch` only after this successful one-time recovery test.


## 2026-11-17 — Completed Apple Silicon exhaustive PyTorch search; FULL NEGATIVE overall result

**Primary evidence:** user-provided Mac terminal log `Pasted text(20261116-221825).txt` (6251 lines), after resuming `output/kyoto-learned-20261116T050002Z-exhaustive`. The raw evidence and `manifest.json`, `trials.jsonl`, `HELDOUT_ACTUAL_RUNTIME.json`, `controller.mlpc`, and all per-frame CSVs live under that original local directory. All **156/156** hyperparameter configurations finished; the final model was exported; real closed-loop native C++ evaluation of the held-out material trajectories finished; `[KYOTO-ML] SUCCESS` was printed and a **complete 4,768-file ZIP** was generated at `output/kyoto-learned-20261116T050002Z-exhaustive.zip`. No failure traceback appears in the ending log. Search best validation SmoothL1-on-log-ms score: `0.239938`.

**Data:** 1,080 original material trajectory snapshots/teacher replays, 100 frames each; held-out runtime report includes **216 material/scale/regime/seed trajectories**, from held-out seeds **101 and 1009** (six material families × six deformation regimes × three target sizes × two seeds). Exact AABB-pair-set/hash checks are reported passing throughout the native replay. Results are isolated CPU broad-phase **query** wall timings including neural features, inference and cache maintenance, not full coupled cloth physics or GPU learned execution.

**Deployed controller vs Global Guard:** geometric mean per-trajectory relative throughput **0.878×**, 95% naive trajectory-resampled bootstrap interval **[0.844×,0.914×]**, 54/216 scenarios faster and 162/216 slower. This CI must NOT be presented as 216 independent physical experiments: material/regime/scale trajectories share only two seeds and one host. By size, relative throughput was 0.842× at 128 requested elements, 0.844× at 512, and 0.953× at 2048; by material, cloth 0.935×, cushion **0.582×**, mixed 0.931×, soft ball 0.996×, sponge 0.848×, spring **1.072×**. The learned controller is **not globally faster** than the strongest single baseline, and no generic asymptotic or future-proof claim is justified.

**Separate comparison against the unchanged generalized material `online_cost_aware` heuristic:** grouped geometric-mean relative throughput **1.233×** over 216 scenes. That is evidence of potential improvement over this *particular generalized heuristic*, NOT improvement over all fixed backends, and NOT a comparison to the original physically coupled cloth-specific `OnlineContactAware` research implementation. Only **17/216** scenes were individually faster than both Global Guard and generalized online. Further direct comparison against Selective Guard, hierarchical region, SAP and Bullet remains necessary before any scientific superiority claim.

**Where it actually helps vs Global Guard:** spring compression at ~2034 true AABBs: 2.325× (seed 1009), 2.208× (seed 101). Cloth compression at 2048 true AABBs: 2.251× and 1.976×, respectively. These are specific speedups on measured workloads, not proof of novel accuracy or overall speed advantage. Fast guarded reuse at sparse/relax scenes and learned feature/switch overhead cause consistent losses: **0/36** cushion scenes and **0/36** relaxation-regime scenes beat Global Guard. This pattern supports a workload-conditional *fallback or controller gating* study, not blanket neural routing.

**Training held-out independent-cache cost prediction, DISTINCT from deployed results:** proxy top-1 cheapest-policy classification `0.76514` on 21,600 frame samples; predicted-choice mean 0.9833 ms, hindsight-oracle lower-bound mean 0.41185 ms; independent static policy mean costs: global 1.31167 ms, hierarchical 0.99240 ms, SAP 1.31194 ms, selective 1.13300 ms. The large proxy regret highlights that predicted *instantaneous* costs under four perpetually warmed teacher caches do not accurately capture cache-reset costs and true controller switching value.

**Scientific interpretation and decision:** Exhaustive architectural search alone did not rescue the policy; extra model capacity/training is not currently the bottleneck. Do **not** run another 156-trial grid on the same two inspected test seeds. Preserve the final artifacts and compare all baselines by material, scale, deformation regime and *physical* contact-work correctness. Next, instrument per-action switching regret and cache invalidation, test cheap deterministic gating/cooldown/analytical-cost and contextual-bandit controls with genuinely untouched seeds, benchmark across independent repeated runs and machines, then perform fair isolated 8k–100k AABB stress tests with bounded RSS/timeouts. Only promote an adapted learned policy to the original coupled Kyoto solver if full-step contact equivalence and end-to-end gains independently validate. This was a successful **experiment execution but an unsuccessful overall performance hypothesis**.


## 2026-11-17 — Audit of full uploaded exhaustive-analysis archive (complete trial rankings and scene aggregates)

**Evidence:** user-provided `kyoto-exhaustive-analysis.zip`, SHA-256 `89637eb6b0ccd9ac026e10722b87b86f50319a2afcc1eb5e863baf0abbfc153b`; files: `manifest.json`, all **156** `trials.jsonl`, 349 `final_training_history.json` epochs, and **216** original scene reports `HELDOUT_ACTUAL_RUNTIME.json/.md`. All counts/aggregates in this entry were independently recomputed from the archive. Full analysis prepared as `kyoto_exhaustive_deep_audit.md` (uploaded in conversation; not copied to git to avoid fabricating raw data in repo).

**Experimental objective reversal, both factually true:** per-trajectory equal-weight **geometric mean** speedup against Global Guard was **0.878164×**, with only 54/216 scene wins. But summing the **mean full query time** across the 216 equal-length trajectory types gives a **1.205485×** total-time ratio in favor of ML: average per scene 1.3810 ms Guard vs 1.1456 ms learned. High-contact workloads dominate absolute runtime; quiet scenes dominate count/multiplicative regret. Neither ratio represents a known deployed workload distribution. Report both, never cherry-pick only one.

**Tail behavior:** geometric mean per-scene **median** Guard/ML ratio **0.7676×**, P95 ratio **0.7787×**; the neural policy was faster in only 28/216 scene medians and 67/216 scene P95s. Against generalized `online_cost_aware` the mean-speed geometric ratio was **1.23285×** (160/216 mean wins), but the P95 ratio was **0.786×**, a serious tail regression. The 216 paired rows are **not 216 independent seeds**: all scenes share just two untouched-at-evaluation seeds 101 and 1009; seed-specific geometric Guard/ML ratios were 0.8820× and 0.8744×. The reported [0.8444,0.9136] scene bootstrap CI resamples correlated rows, and must NOT be interpreted as uncertainty across independently generated seeds.

**Controller action mix after warmup:** 19,872 timed frames = Guard **17,123 (86.2%)**, hierarchical region 959 (4.8%), selective guard 900 (4.5%), SAP 890 (4.5%). Total **292** backend switches; 75/216 trajectories had zero switches. On all 72 requested-128-element trajectories, neural inference bypassed and Guard was always selected, with zero switches, yet learned wrapper still scored only **0.842×** versus direct Guard. Source inspection confirms extra `primitives(boxes)` creation, `rawBounds(input)`, `padded(raw)`, and pair-list copying/filtering in the learned wrapper. This suggests a concrete zero-extra-copy direct-Guard bypass ablation; causal attribution requires instrumented same-frame comparisons.

**Regime/scale:** compression at requested 2048 was **1.336×** (10/12 scene wins); rapid 2048 **1.070×** (9/12); relaxation at 128/512/2048 respectively **0.753/0.597/0.670×**, with **zero wins out of all 36 relaxation scenes**. Cushions at requested 128/512/2048 scored 0.687/0.536/0.536×, with **zero wins out of 36 cushion scenes**. This is observational workload heterogeneity, not a validated causal gate. Spring 2048 scores 1.299× overall but spring-compression gain 2.325× vs Guard is actually matched/slightly bettered by always-on hierarchical region.

**Hindsight fixed-policy comparator:** ML outperformed every independently warmed **static** method in only **4/216** scenes. Two 128-budget no-switch Guard-only scenes had tiny 0.5% and 1.3% advantages, a third high-N sponge-compression had ~0.9%, and only sponge rapid high-N (seed 101) had a nontrivial ~11.3% advantage over the best standalone region-adaptive backend. Treat that one as a replication hypothesis; do not claim superior online adaptive algorithm from one seed.

**Overfitting and objective distinction:** winner was trial 68 (2×32, lr=0.001, decay=0.0001, dropout=0, batch=64), validation Huber 0.239938 at epoch 349; all top 12 trials were the same 2×32 no-dropout family, differing only by optimizer configuration. Training full trial wall-clock time summed to 12.90 hours. Test proxy teacher-cost top1 was 76.51%, **but** mean actual warmed-teacher cost of chosen action 0.9833 ms versus impossible framewise hindsight oracle 0.4118 ms; decision regret 0.5715 ms. Model's mean predicted minimum 0.3970 ms suggests inadequate cost calibration. Teacher target costs from separately warmed caches ignore destination invalidation and switching. `validation_post_refit` is **in-sample** after train+validation are combined; its similar accuracy to test does NOT independently rule out overfitting.

**Data boundary:** uploaded analysis ZIP does not include original per-frame C++ CSVs, so the reported 216/216 exact-pair outcomes and per-scene switching counts can be audited at aggregate level, but not reverified independently from full pair hashes, action traces or feature/inference/decision/backend timing. Preserve all raw data locally. New experiments must use new held-out seeds, a predeclared workload mix, equivalent timing boundaries, switch-penalty calibration in measured milliseconds, independent machines, and coupled-physics validation.

**Next action:** pause new 156-trial training; first compare true direct-Guard/no-copy bypass, per-component overhead, no-switch ablation, and a cheap causal non-neural cost gate on **fresh unseen scenes**. Only evaluate contextual bandit or long-horizon RL after actual switched reward demonstrates a persistent advantage, with full oracle parity and repeated seeded P95 evidence.


**Comparator terminology clarification:** The `evaluate_learned_replay.py` field `ml_vs_best_static_posthoc_mean_speedup` is misnamed: its comparator set includes the standalone **dynamic** generalized `online_cost_aware` heuristic, as well as static SAP/Guard/regional/Bullet backends. Thus the 4/216 result means “faster than every separately run baseline policy in that trajectory,” *not* exclusively static algorithms. Future report output should rename or document this field without changing its numeric semantics.

## 2026-11-17 — New exact persistent SAP baseline and fresh-seed hybrid-controller ablation

**Changes committed:** `apps/material_replay_benchmark.cpp` now includes `sap_persistent_coherent`, a distinct persistent Sweep-and-Prune baseline retaining previous x-axis endpoint order across deforming frames. It uses stable primitive ID verification, bounded insertion-order repair (maximum 8N shifts) with a full sort fallback on adversarial reorder, 3D inclusive AABB overlap and symmetric category/mask filtering, canonical sorted unique ID pairs, and the exact same material-replay pair hash oracle as the other methods. This is not integrated as a trained MLP output (the existing MLP has four outputs); therefore cannot claim the learned controller exploits temporal SAP yet. The event-driven kinetic-SAP literature is a more complex research direction, not an implementation claim for this baseline.

**Evaluation changes:** `scripts/evaluate_learned_replay.py` includes persistent SAP among posthoc individual baselines; `scripts/run_kyoto_controller_ablation.py` reports fresh-seed persistent SAP vs Guard and current ML variants. `scripts/test_learned_cxx.py` now verifies exact pair hashes in material-impact (12 frames) and rapid mixed deformation (20 frames). Existing direct-Bounds/no-copy learned replay and a guard-first conservative MLP variant remain separate previously implemented ablations, not invented wins.

**Motivation from sources:** the [Broadmark benchmarking paper](https://diglib.eg.org/bitstreams/d1ac9824-285d-4dfa-8184-17e165bdacfd/download) distinguishes fresh SAP and incremental SAP and warns against narrow benchmark choice; [SOFA Sweep and Prune documentation](https://sofa-framework.github.io/doc/components/collision/detection/algorithm/detection-sweep-and-prune/) describes persistent internal SAP state versus direct output; [Adaptive Collision Culling via Context-Aware SAP](https://pubmed.ncbi.nlm.nih.gov/28574360/) describes adapting axes/partitioning for clustered distributions. These sources motivate experiments, not measured improvements or established novelty of this simple implementation.

**Critical limitations:** This commit has not yet been benchmarked on the user's Apple Silicon machine nor on unseen held-out material seeds. No numerical speedup claim follows from code changes. The current 156-trial model has seen neither this new backend as a label nor action-specific destination cache miss penalties in its supervised targets. Absolute dominance of all separately warmed baselines in every scene is not assured or generally possible with nonzero selection overhead and unknown future motion; optimization should instead target predeclared workload-weighted mean/P95 latency with guaranteed exact-pair output, documented worst-case fallback, and evaluation on fresh datasets.

**Reproducible Mac commands (do not overwrite original run):**
```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git restore --source=origin/research/kyoto-scale-evidence -- apps/material_replay_benchmark.cpp scripts/evaluate_learned_replay.py scripts/run_kyoto_controller_ablation.py scripts/test_learned_cxx.py
source .venv-kyoto-ml/bin/activate
cmake --build build/kyoto-material-macos --target ccp_material_export ccp_material_replay --parallel 4
ctest --test-dir build/kyoto-material-macos --output-on-failure -R learned_cpp_model_and_exact_pair_oracle
python scripts/run_kyoto_controller_ablation.py --model output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc --output output/kyoto-coherent-pilot-20261117 --max-experiments 36
```
Only after independently inspected correctness and pilot performance should a larger prespecified fresh-seed matrix be considered. Avoid original held-out seeds 101/1009, avoid treating paired frames as independent seeds, and retain separate positive and negative results.


## 2026-11-17 — Mac pilot build failure, stale-binary replay and fail-closed recovery

**Observed in user-uploaded Mac build log `Pasted markdown(20261116-231333).md`:** CMake failed building `apps/material_replay_benchmark.cpp` because the user's local `include/ccp/learned_online.hpp` was left on an older revision. The then-current benchmark invoked `LearnedOnline(model,0.,.10,.30,24,true)` and `learned_->query(boxes)`, while the local header only accepted five constructor arguments and a `std::vector<Primitive>` query. The previous update command had **omitted the header**, a concrete instruction error. Third-party Bullet compound-literal warnings were unrelated; the two type errors caused build failure.

**After build failure:** CTest ran against an OLD surviving `ccp_material_replay` binary and failed the new `ml_bookkeeping_ms` schema check. The pilot script nevertheless exported/replayed all 36 scenes with that surviving executable. Uploaded `FRESH_SEED_RESULTS.json` from four fresh seeds (1201, 1301, 1409, 1511) gave **0.8693999×** per-trajectory Guard/old ML geomean, **8/36** mean wins, **1.1024954×** aggregate total scene mean Guard/old ML ratio. Its nine methods OMIT `sap_persistent_coherent` and `ml_cost_aware_conservative`, conclusively proving the new benchmark was NOT evaluated. Final script raised `missing new timing field ml_bookkeeping_ms`. Preserve this pilot as **legacy/exploratory** and NEVER claim that persistent SAP or the new direct-Bounds/conservative ablation achieved these numbers.

**Fail-closed repair committed:** C++ `ccp_material_replay --capabilities` returns a versioned contract (`KYOTO_REPLAY_ABLATION_V2` plus the required method/metric names). `scripts/run_kyoto_controller_ablation.py` probes this contract *before creating output or exporting scenes*, compares replay executable timestamp against modified C++ sources, records exporter/replayer SHA-256 hashes in the immutable ablation contract, requires all current columns and baseline policies for newly generated OR resumed CSVs, and validates the component-timing results before publishing final reports. A stale existing report stops with a clear instruction to use a **new output directory**; original evidence is never removed or overwritten.

**Regression protection:** `scripts/test_ablation_preflight.py` contains five tests (reject old executable, accept current capability, reject old missing-column CSV, reject absent coherent-SAP backend, accept complete schema); `scripts/test_learned_cxx.py` now checks the real replay binary contract before its exact-pair oracle fixtures. The normal GitHub Actions cpu-reference job includes these tests. These changes were committed; successful macOS revalidation and any new speedup measurements are still **pending**, not claimed.

**One-time safe user repair commands, preserving uncommitted training scripts and original data:**
```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git restore --source=origin/research/kyoto-scale-evidence -- \
  include/ccp/learned_online.hpp apps/material_replay_benchmark.cpp \
  scripts/test_learned_cxx.py scripts/evaluate_learned_replay.py \
  scripts/run_kyoto_controller_ablation.py scripts/test_ablation_preflight.py
source .venv-kyoto-ml/bin/activate
cmake --build build/kyoto-material-macos --target ccp_material_export ccp_material_replay --parallel 4 && \
ctest --test-dir build/kyoto-material-macos --output-on-failure -R learned_cpp_model_and_exact_pair_oracle && \
python -m unittest scripts.test_ablation_preflight -v && \
python scripts/run_kyoto_controller_ablation.py \
  --model output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc \
  --output output/kyoto-coherent-pilot-20261117-verified \
  --max-experiments 36
```
Do not pass `--resume` with the old 36-CSV folder: it contains an obsolete evidence contract and schema. The guarded command chain stops on build/test failure.


## 2026-11-17 — Second local Mac sync failure: generic Result ABI field omitted

The user restored `include/ccp/learned_online.hpp`, `apps/material_replay_benchmark.cpp`, and evaluation scripts but **did not restore `include/ccp/generic_online.hpp`**, which declares `ccp::bench::Result::mlBookkeepingMs` on the remote research branch. Xcode therefore failed compilation with two concrete errors at `learned_online.hpp:155` and `material_replay_benchmark.cpp:288`: `no member named 'mlBookkeepingMs' in 'ccp::bench::Result'`. Unrelated Bullet C99 compound-literal warnings are not the cause. Because the guarded `&&` command stopped on failed build, **no new verified ablation was generated**.

**Root cause:** another incomplete manual restore command, not a missing declaration in the remote source; GitHub's full source already contains the correct field. The full remote cpu-reference workflow [43978445834](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43978445834) completed successfully including C++ build and regression tests at branch head `8df423f`. Expanding stale-binary checks to include `generic_online.hpp`, `learned_policy.hpp`, and `live_cloth.hpp` is committed separately. None of this establishes a macOS speedup yet.

**Minimal safe remedy** (do not reset local training files, experiments, or locally edited DEVLOG):
```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git restore --source=origin/research/kyoto-scale-evidence -- include/ccp/generic_online.hpp scripts/run_kyoto_controller_ablation.py
cmake --build build/kyoto-material-macos --target ccp_material_export ccp_material_replay --parallel 4 && \
ctest --test-dir build/kyoto-material-macos --output-on-failure -R learned_cpp_model_and_exact_pair_oracle && \
python -m unittest scripts.test_ablation_preflight -v && \
python scripts/run_kyoto_controller_ablation.py \
 --model output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc \
 --output output/kyoto-coherent-pilot-20261117-verified --max-experiments 36
```


## 2026-11-17 — Verified 36-scene Mac pilot; guard/SAP measured-probe hybrid exploratory baseline

**Primary evidence:** user-uploaded `Pasted markdown(20261116-232610).md`, `FRESH_SEED_RESULTS(1).json` and `COMPONENT_TIMING.json`. Mac successfully compiled current `ccp_material_replay`, passed the actual `learned_cpp_model_and_exact_pair_oracle` CTest and all five preflight tests. New complete output: `output/kyoto-coherent-pilot-20261117-verified`. 36 scenes, 4 fresh (at that time) seeds `1201,1301,1409,1511`, 92 timed frames each (total 3312) across 6 materials and 6 deformation regimes but nonuniform random coverage (e.g. rapid only 1/36). Every algorithm's candidate-pair hash matched the independent exact replay reference on every tested frame.

**Observed results versus direct Global Guard**, per-trajectory geometric means: original ML **0.897686×** (11/36 wins); Guard-first conservative ML **0.973629×** (12/36 wins); coherent persistent SAP **0.479104×** (9/36 wins); conservative versus original ML **1.084599×**. Aggregate ratio of summed 36 scene mean costs: ML **1.109035×**, conservative ML **1.136092×**, persistent SAP **0.744959×** against Guard. These measures weight workloads differently. The 95% scene bootstrap of original MLP was [0.8104,0.9931], but 36 scenes share only four seeds; this is NOT a seed-independent CI.

**Tail:** P95 per-trajectory geometric means versus Guard: original MLP **0.8314×** (15/36 P95 wins), conservative MLP **0.9723×** (17/36), coherent persistent SAP **1.0773×** (27/36). Persistent SAP may be promising for reducing Guard's costly rebuild spikes, notwithstanding its poorer mean on low-motion scenes. On cloth compression 2048 true AABBs, Guard 5.1563ms, coherent SAP 1.8042ms (**2.858×**); cloth impact 2048, Guard 2.538ms, coherent SAP 1.4077ms (**1.803×**). Other baselines can win other scenes; Global Guard is the fastest separately run policy on 20/36 of the pilot.

**Measured ML costs per timed frame:** causal feature generation **9.5035 microseconds**, neural prediction **0.7662 microseconds**, decision logic **0.02356 microseconds**, post-backend bookkeeping **1.675 microseconds**, outer-wrapper **2.4535 microseconds**. Backend core work mean **0.852583 ms** across heterogeneous scenes. The original ML selected direct Guard 2885/3312 frames (**87.1%**), bypassed neural inference for 828, and switched 46 times. Thus refining the model's hidden-layer count is unlikely to be the most effective immediate next experiment.

**Hindsight ceiling:** selecting the fastest separately warmed baseline per *whole* scene would achieve ~**1.3469×** aggregate-time ratio versus Guard on this biased 36-scene set, or 1.1271× per-scene geometric ratio. These are unrealizable oracle upper bounds *for a particular comparator set*, not a deployment or general theoretical bound. The evaluator retains legacy `ml_vs_best_static_posthoc_mean_speedup` for compatibility but adds the precise `ml_vs_best_competing_policy_posthoc_mean_speedup`: the comparator set includes dynamic generalized-online and measured-probe variants, not just static algorithms.

**New exploratory benchmark-only policy:** `guard_sap_measured_probe` in `apps/material_replay_benchmark.cpp`. Starts with exact Guard; for >=384 AABBs samples up to 32 current raw boxes against existing Guard envelope certificates; when >=25% sampled boxes escape or last observed Guard query exceeds 0.5ms, samples the current costs of Guard and independently maintained persistent SAP no more often than every 24 frames. Both algorithms execute on probe frames, **both costs are charged** in the method's measured wall time, switching requires 12% improvement, and all outputs go through the complete per-frame pair oracle. It never uses future frames, material names, or teacher prediction labels. This simple causal rule may fail on sudden workload changes and is NOT represented as novel by itself. Outputs `hybrid_probed` and `hybrid_switched` per frame, plus aggregate counters and comparisons.

**Data-design repair:** `run_kyoto_controller_ablation.py --sampling balanced` samples every material×regime×size cell once before repeating any cell with a different seed; SHA-stamped replay ABI `KYOTO_REPLAY_ABLATION_V3` prevents mixing binaries and requires hybrid output columns. Unit tests verify balanced coverage and distinct seed repeats. Earlier `--sampling random` remains the default for legacy reproducibility. CI runs include the C++ exact oracle and Python tests on actual source; the user has NOT yet run this hybrid on Mac and **no improvement is claimed**. Keep the former 36 scenes as already-inspected exploratory development data.

**Next independent Mac study** (new seeds not previously analyzed; new output; no training):
```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git restore --source=origin/research/kyoto-scale-evidence -- \
 apps/material_replay_benchmark.cpp scripts/test_learned_cxx.py \
 scripts/test_ablation_preflight.py scripts/run_kyoto_controller_ablation.py \
 scripts/evaluate_learned_replay.py
cmake --build build/kyoto-material-macos --target ccp_material_export ccp_material_replay --parallel 4 && \
 ctest --test-dir build/kyoto-material-macos --output-on-failure -R learned_cpp_model_and_exact_pair_oracle && \
 python -m unittest scripts.test_ablation_preflight -v && \
 python scripts/run_kyoto_controller_ablation.py \
  --model output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc \
  --output output/kyoto-guard-sap-balanced-20261117 \
  --seeds 1709 1801 1907 2003 --sampling balanced --max-experiments 108
```
Report P95, per-material/regime/budget performance, exact oracle parity, probes/switches, and overhead; if the hybrid loses, keep that result and investigate. Neither hybrid nor existing MLP is yet shown to win in every case or integrate into coupled cloth solving.


## 2026-11-17 — Balanced 108-scene new-seed Guard/SAP hybrid pilot COMPLETED; negative geomean

**Primary evidence:** user-provided Mac terminal log `Pasted markdown(20261116-234324).md` (857 lines). `git fetch` advanced to commit `3b06740`, and the restored headers, C++ replay, Python runner and regression tests all built. `learned_cpp_model_and_exact_pair_oracle` passed; six Python preflight tests passed. The user successfully completed **108/108** fresh-seed material trajectories with deterministic `--sampling balanced`, seeds `1709 1801 1907 2003`, under `output/kyoto-guard-sap-balanced-20261117`. The original exhaustive data and both older pilots were not modified. One full material × motion-regime × size cell is represented for every 108-cell combination, **but each cell has only one of the four seeds**, not four independent replicates.

**Observed aggregate logs only (scene JSON/CSV not yet uploaded):** versus direct Global Guard, per-trajectory equally weighted **geometric mean throughput ratio** original 4-action MLP **0.9301×**, conservative Guard-first MLP **0.9825×**, persistent coherent SAP **0.4864×**, and *new causal measured-probe Guard/SAP hybrid* **0.9006×**. Conservative MLP / original MLP geomean ratio **1.0563×**. Original MLP / coherent SAP **1.9122×**. Summed scene mean-time Guard / original MLP **1.2274×** (different aggregation; may reward high-cost workloads). The **new hybrid did not exceed Guard, conservative MLP or default MLP by typical-workload geometric mean**. This is a legitimate **negative overall result**, not an innovation success claim.

**Action counts** (warmup excluded): hybrid performed **212 cost probes and 18 switches**; original MLP switched **137** times. Total timed frames expected **108×92 = 9,936**. The new hybrid's probe incidence is ~**2.13%** of timed frames, which does not on its own prove that probes caused the loss. No JSON aggregate total-time for hybrid, per-scene P95, per-material/regime rankings, fallback usage, or component-timing breakdown was supplied in this message. Do not invent those metrics or infer that the hybrid improves tails from the old 36-scene SAP P95 result.

**Research decision:** Preserve benchmark results, do NOT tune thresholds on this already inspected evaluation set and describe it as unseen confirmation. Before implementing another controller revision, inspect the 108-scene `FRESH_SEED_RESULTS.json`, `COMPONENT_TIMING.json`, and relevant per-frame CSVs with `hybrid_probed`, `hybrid_switched`, `chosen_backend`, and `ms`; quantify where wins/losses arise (quiet Guard-only overhead, false/late trigger, switching regret, expensive double-probe frames). Evaluate any new policy on truly new seeds and repeated hardware runs. Aim at cache-aware *single-pass* certificate gating, amortized probes and strong exact-pair/full-physics equivalence; no general dominance claim warranted.

