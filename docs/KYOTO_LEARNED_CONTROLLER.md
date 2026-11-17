# Kyoto learned online broad-phase controller — research protocol

## Purpose, hypothesis and non-negotiable correctness

The **original Kyoto scientific target remains Online Contact-Work-Aware Broad-Phase** for real deformable, coupled collision response. This research extension introduces a *learned backend selector*, not a black-box neural collision detector. The MLP never generates or rejects individual pairs. It picks from four already-certified candidate generators:

`sap_rebuild`, `global_guarded_exact`, `selective_guard_exact`, and `region_hierarchical`.

Every deployed material replay compares the complete stable-ID candidate-pair hash and cardinality with exact SAP *for every frame*. The network is an optional policy for the **generalized material AABB workload**, under the key `ml_cost_aware`. The **original 12x12 contact-work-aware cloth solver is not silently replaced**, and the Metal region-span heuristic is not a GPU version of this PyTorch model. We will only port a proven policy to the original solver after an isolated held-out win with audited physics equivalence.

Research hypothesis: low-cost, causal geometry/motion statistics let a tiny neural estimator select an appropriate safe cache strategy and outperform fixed policies on diverse held-out deformation, after fully accounting for feature construction, inference, cache invalidation and strategy switching.

## Learnable representation

The model uses exactly twelve **causal** features, computed from current padded AABBs and previous observed AABBs/pairs:

1. log primitive count
2. normalized mean object AABB extent
3–5. normalized scene aspect ratios
6. log volume occupancy (sum of AABB volumes / enclosing scene volume)
7. normalized positional variance
8. mean normalized center displacement since previous snapshot
9. maximum normalized center displacement
10. fraction of moving primitives
11. previous-frame pairs per object
12. absolute change in previous-frame pair density

No future-frame labels, oracle candidates, current algorithm timings, scene material names or IDs are model inputs. The same feature code, `ccp::learned::Extractor`, creates training CSV features and is used by the native runtime. Model preprocessing means/stds come **only** from training trajectories (and from train+validation during final refit). Each action predicts natural log CPU query milliseconds.

A full `Linear(12,16)-ReLU-Linear(16,16)-ReLU-Linear(16,4)` model has **548 parameters** (12×16 +16 +16×16+16 +16×4+4). The grid also includes a linear perceptron and smaller/deeper alternatives. (Parameter calculations use the actual selected architecture.) The default test baseline must always include no-ML Global Guard and heuristic Online.

## Supported hyperparameter search

`scripts/train_online_controller.py` implements a deterministic **156-configuration exhaustive grid**:

- linear (0 hidden), 1×8/16/32 hidden, 2×8/16/32 hidden
- learning rates 0.0003 / 0.001
- AdamW weight decay 0 / 0.0001 / 0.001
- batch sizes 64 / 256
- dropout 0 or 0.1 for hidden models; none for linear

Use `--search grid --max-trials 32` for a bounded shuffled grid or `--exhaustive` for all 156 configurations. `--search random` uses a fixed-seed sample from the same grid; `--search single` allows an architecture sanity check. No unrequested third-party scheduler or invisible tuning occurs.

Training minimizes SmoothL1 on **log measured query ms**, uses fixed RNG seeds, gradient clipping, checkpoint selection by *validation loss*, patience-based early stopping, and records every validation trial in append-only `trials.jsonl`. Once the best hyperparameters and epoch are fixed, the final PyTorch model is refit on train+validation for that preselected epoch count, and the untouched test split is assessed **once**. Artifacts: `controller.mlpc` (versioned native inference weights), `controller.state_dict.pt`, `manifest.json`, and loss history. The model must not be deployed based only on training loss.

This is **supervised cost prediction**, not reinforcement learning. Contextual bandits or RL are subsequent controlled ablations only if delayed cache rebuilding and state-dependent returns prove important on real held-out workloads.

## Critical limitation of supervised labels

Teacher observations come from running each algorithm's own continuous cache on the *same* snapshot series. The labels represent counterfactual instantaneous cost **with each teacher's own warmed cache**. A switched learned controller cannot magically inherit the unused teacher's cache state; on each switch, its destination cache is explicitly invalidated, and the next measured query pays the rebuild. A cooldown and switching penalty reduce thrashing; all such work is timed.

The validation metric *"oracle chooses cheapest teacher every frame"* is a hindsight lower bound and must never be presented as a deployable speedup. Report separately:

- Validation/test proxy top-1 cost selection and regret.
- **Measured actual C++ learned-switching query time**, including inference, feature construction, cache resets and full pair materialization, versus fixed SAP, global guard, selective guard, regional and unchanged original generalized Online.
- Strict current pair count/hash equality every replay frame; reference SAP sampled against exhaustive for small scenes.
- Runtime median, P95, number of changes, cache builds/repairs/reuses, memory, and total wall/query boundaries.
- Independent material/seed/regime results, including losses; 95% bootstrap resampling **entire trajectories** rather than treating adjacent frames as IID.

## Quick Mac run

First update and build the latest native branch after earlier Mac SDK corrections:

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git pull --ff-only origin research/kyoto-scale-evidence
KYOTO_VERBOSE=1 bash scripts/run_kyoto_materials_mac.sh smoke
```

For PyTorch, prefer a separate virtual environment (the user previously had Anaconda CMake shadow Homebrew's compiler). Create a local venv with system/Homebrew Python and install only essential packages:

```bash
python3 -m venv .venv-kyoto
source .venv-kyoto/bin/activate
python -m pip install --upgrade pip
python -m pip install torch numpy
python scripts/run_kyoto_ml_mac.py smoke
python scripts/run_kyoto_ml_mac.py quick
```

A **full scientific sweep** runs:

```bash
python scripts/run_kyoto_ml_mac.py standard --max-trials 48
```

For all 156 hyperparameter configurations plus a larger multi-seed scene matrix:

```bash
python scripts/run_kyoto_ml_mac.py exhaustive
```

This last option is intentionally expensive: it generates many complete geometric trajectories and trains many networks, with per-command timeouts and full logs. Start with `smoke` and `quick` to verify PyTorch, native ABI and correctness. In a supported environment the wrapper runs CPU teacher data collection (including Bullet where installed), PyTorch search, held-out **actual** C++ policy timing, trajectory-level confidence intervals and append-only `DEVLOG.md` entries.

Output is `output/kyoto-learned-<UTC>-<mode>/`, plus a ZIP (even on failure) via `scripts/package_kyoto_results.py`. Upload the ZIP. Raw snapshots, per-frame CSV, trial logs and complete metadata are required to claim any gain.

## Resuming the November 16 Mac exhaustive run (do not regenerate data)

The user's Mac logs establish that **quick and standard completed**. Their post-success `TypeError: expected str, bytes or os.PathLike object, not int` was a ZIP-packaging callback bug, fixed on the research branch. The standard 96-scene holdout geometric-mean throughput relative to Global Guard was **0.917×**, so no learned overall win is claimed.

The **exhaustive** experiment `output/kyoto-learned-20261116T050002Z-exhaustive` has 1,080 immutable teacher/snapshot pairs and at least **34 of 156** trial records. The log ending at trial 34 is **not itself a traceback or proof of a frozen process**. The old trainer only printed after whole trials. First check whether the original Python process is still running:

```bash
ps -axo pid,etime,%cpu,%mem,command | grep -E '[t]rain_online_controller.py|[r]un_kyoto_ml_mac.py'
wc -l output/kyoto-learned-20261116T050002Z-exhaustive/model/trials.jsonl
tail -n 3 output/kyoto-learned-20261116T050002Z-exhaustive/logs/torch-training.log
```

If a training process remains active, **do not launch another one**. The process may be spending substantial time on a 500-epoch configuration using PyTorch's MPS backend; inspect Activity Monitor, CPU/GPU usage and the live log before choosing to interrupt.

If the previous training process has **fully exited**, update just the recovery scripts. This route preserves any locally appended/uncommitted `DEVLOG.md`:

```bash
git fetch origin research/kyoto-scale-evidence
git restore --source=origin/research/kyoto-scale-evidence -- \
  scripts/run_kyoto_ml_mac.py \
  scripts/train_online_controller.py \
  scripts/package_kyoto_results.py

python scripts/run_kyoto_ml_mac.py exhaustive \
  --resume-run output/kyoto-learned-20261116T050002Z-exhaustive
```

This does not delete, re-export, or overwrite the 1,080 complete snapshots or teacher CSVs. It compares the existing trial indices and full hyperparameters against the original deterministic shuffled grid, then restarts **the first incomplete trial** (trial 35 if precisely 34 have been completed). From then onward it prints `[TRAIN-EPOCH]` diagnostics every 25 epochs, followed by `[TRAIN]` results for completed trials. New trials append durably to `model/trials.jsonl`. Original logs append rather than being truncated. The Python trainer also rejects changes to previously recorded training settings and dataset hashes after creating its search contract.

If the exhaustive run is interrupted again, a **small diagnostic-only ZIP** is written beside the run directory while the large snapshots and full teacher data remain untouched and resumable. On success, the original full evidence ZIP is produced with visible packaging progress. A full ZIP may be substantial for an exhaustive experiment.

To package *already complete* quick/standard runs without rerunning their training:

```bash
python scripts/package_kyoto_results.py \
  output/kyoto-learned-20261115T195351Z-quick --exit-code 0
python scripts/package_kyoto_results.py \
  output/kyoto-learned-20261115T195639Z-standard --exit-code 0
```

If sharing only debugging logs rather than every original binary snapshot, append `--diagnostics-only` to create a smaller diagnostic ZIP. That smaller archive is **not complete raw evidence**; source data remain in the experiment directory.

## Isolated large-scale stress experiments

The claim that opponents "fail at too many elements whereas ours is futureproof" is a **hypothesis**, never a preset conclusion. For fair comparisons, use `scripts/run_kyoto_stress.py`, which runs **one exact algorithm per isolated process** with the same material, dynamic regime, seed, frame count, per-process timeout and sampled RSS cap, and independent pair coverage checks. 

```bash
python scripts/run_kyoto_stress.py quick --max-seconds-per-policy 180 --max-rss-mb 4096
python scripts/run_kyoto_stress.py stress --max-seconds-per-policy 240 --max-rss-mb 4096
python scripts/run_kyoto_stress.py extreme --max-seconds-per-policy 300 --max-rss-mb 6144
```

The `extreme` range explores **16,384–100,000 requested elements**. Actual AABB count varies by material and can exceed this count (especially triangulated soft-ball surfaces). Extremely dense scenes have an unavoidable \\(\\Omega(N^2)\\) output-size lower bound when all overlapping pairs must be enumerated; no exact-list algorithm can make that disappear. Some experiments may time out or hit memory caps. These are **censored observations** and must not be misrepresented as an algorithmic superiority proof. RSS is sampled at 100 ms intervals and may undercount short-lived allocation spikes. CPU stress does not prove a GPU port scales that far.

The stress runner writes every attempt to `manifest.json`, preserving failures, partial per-frame CSVs and diagnostic logs. It appends the actual status matrix to `DEVLOG.md`. Package stress evidence as:

```bash
python scripts/package_kyoto_results.py output/<specific-stress-directory>
```

## Acceptance gates

1. The model's exported native weights must be loadable, dimensionally correct and deterministic. Corrupt model files must be rejected.
2. *All* held-out learned candidate pair sets must match the independent SAP reference; memory/timeout failure is not a passed trial.
3. Actual learned runtime must beat the best relevant **deployable** static/heuristic competitor across independent test groups, **with inference and switching overhead included**; report cases where this is false.
4. Training metrics never imply verified future gains. Retain all negative runs, cache invalidations and changed models in `DEVLOG.md`.
5. The original coupled `MultiCloth` physics test, not only material AABB replay, must independently reproduce equivalent contact response before learned selection may become the default online research algorithm.

## 2026-11-17: no-retraining optimized learned policy and fresh-seed A/B experiment

After the original **156/156** grid search, the actual Mac test showed **0.878× geometric-mean Guard/ML speedup** over 216 trajectories, **54 wins and 162 losses**, despite aggregate-summed scene means showing a 1.205× relative time ratio in the opposite direction. Those original two final-test seeds (101, 1009) are now **consumed** and must not be recycled to validate modifications. The complete original data and model in the user's Mac directory remain unchanged.

### Changes made without modifying the neural weights

1. C++ `LearnedOnline` now accepts raw `std::vector<Bounds>` directly, avoiding the pointless `Bounds -> Primitive -> Bounds` conversion in material replay. When `margin==0`, it also avoids constructing a duplicate padded bounds vector. The existing `Primitive` caller remains supported, preserving its original sheet metadata for regional policies.
2. The MLP inference is skipped during a mandatory cooldown when no backend change is permissible. Causal feature history is still updated every frame; this is a performance-only change to default policy decisions.
3. The evaluator adds `ml_bookkeeping_ms` (feature-history update and post-backend work) and `ml_wrapper_ms` (external C++ wrapper overhead) to per-frame CSVs. These previously unaccounted components remain part of the **total `ms`**. Existing historical metrics and other backends are not silently changed.
4. A second, **experimental**, predeclared `ml_cost_aware_conservative` policy is replayed alongside `ml_cost_aware`. It begins with verified Global Guard, uses a fixed 0.10 ms switching penalty, 30% predicted-cost hysteresis, and a 24-frame cooldown. These are **hypotheses**, not tuned winner values. Both variants run through the same mandatory per-frame pair-hash oracle.
5. `scripts/test_learned_cxx.py` now checks original/optimized C++ model loading, 128-primitive no-inference fallback despite a model favoring region, 512-primitive hierarchical path, conservative mode, and malformed weights. This native CTest is part of **automatic** CPU reference CI, without requiring expensive PyTorch installation.

### Mac command: test against NEW seeds, no repeat of 13-hour grid

Update to this research revision, then build the two existing native C++ binaries:

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git switch research/kyoto-scale-evidence
git pull --ff-only
cmake --build build/kyoto-material-macos \
  --target ccp_material_export ccp_material_replay --parallel 4
```

Launch a predeclared 36-experiment **fresh-seed exploratory pilot**, with new immutable geometry data and no training:

```bash
source .venv-kyoto-ml/bin/activate
python scripts/run_kyoto_controller_ablation.py \
  --model output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc \
  --build build/kyoto-material-macos \
  --output output/kyoto-fresh-controller-pilot-20261117 \
  --seeds 1201 1301 1409 1511 \
  --max-experiments 36
```

To evaluate every combination (4 seeds × 6 materials × 6 regimes × 3 budgets = **432** trajectories), pass `--max-experiments 0` and use a **different new output directory**. The script writes a model-SHA256 and selection contract **before** running, saves every source snapshot and per-method query CSV, validates every sampled frame's exact pair hashes, then emits `FRESH_SEED_RESULTS.json/.md` and `COMPONENT_TIMING.json`. It refuses to overwrite prior original run data and supports `--resume` only if its contract is identical. A 36-case pilot is not a statistically sufficient generalization study; for paper-level claims increase fresh seed count, independent machines, material generators and physics validation. A stronger overall result is not guaranteed.

### Missing baselines and scientific novelty boundary from literature check

Do not claim a generic adaptive broad-phase or fat-AABB certificate as new: key prior work includes:

- Daniel J. Tracy, Samuel R. Buss, Bryan M. Woods, *Efficient Large-Scale Sweep and Prune Methods with AABB Insertion and Removal*: segmented temporal-coherence SAP for sparse changes (https://mathweb.ucsd.edu/~sbuss/ResearchWeb/EnhancedSweepPrune/SAP_paper_online.pdf). The separate CPU scaling executable has an incremental SAP mode, but **the heterogeneous material replay does not yet compare an incremental SAP method**. This is a high-priority missing competitor.
- Box2D dynamic AABB tree (https://box2d.org/documentation/group__tree.html) and Bullet persistent DBVT (already optionally included in material replay) are published/practical dynamic hierarchy references. Include repeatable dynamic-tree and hardware/memory comparators in future work.
- Daniel et al., *Adaptive Collision Culling for Massive Simulations by a Parallel and Context-Aware Sweep and Prune Algorithm* (https://pubmed.ncbi.nlm.nih.gov/28574360/): multi-axis adaptive sweep under dense clustering, a stronger comparison target than naive SAP.
- David Belgrod et al., *Time of Impact Dataset for Continuous Collision Detection and a Scalable Conservative Algorithm* (https://continuous-collision-detection.github.io/): conservative continuous-collision correctness and broad-/narrow-phase evaluation. Our current AABB-overlap oracle validates **discrete frames**, NOT conservative swept CCD.
- Mohammed Alaa Ala'anzy and Olzhas Bazarkhan, *Solver-Integrated Deformation-Aware BVH Updates for Real-Time Soft-Body Simulation* (IEEE TVCG, 2026, DOI 10.1109/TVCG.2026.3729180): integrates bound updates into solver deformation, reducing redundant full-mesh scans (https://pubmed.ncbi.nlm.nih.gov/42672017/). This is particularly relevant to the long-run Kyoto **coupled-physics** objective but is NOT a result measured in this repository.

**Decision gate:** Improve zero-copy query overhead first. Compare original vs conservative on new seeds. Next implement and fairly compare temporal-coherent SAP/dynamic BVH, measure all backend-switch invalidation cost, then evaluate GPU/large-N and full physically coupled contact workloads. Do not claim performance beyond measured inputs. No universal promise of beating the individually fastest algorithm on every configuration is defensible, particularly with a nonzero controller decision overhead and identical exact-pair outputs.
