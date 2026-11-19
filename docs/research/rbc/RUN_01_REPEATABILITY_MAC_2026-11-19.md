# Final Run 1 / 5 — Mac repeated query comparison (19 Nov 2026)

**Status: COMPLETE. Four native Mac experiment batches remain.** This is a descriptive development-cohort outcome, not an independent held-out result or a demonstrated significant speedup.

## Verified from the user-provided native terminal log

- Build: native C++ `ccp_rbc_final_repeated` compiled successfully in Release.
- Source: existing `output/rbc-observatory-native-pilot-01/RBC_RESULTS.json` manifest, **36 previously inspected source trajectories**, not newly sampled sources.
- Methods: 11, including Guard A/A, skin-parameter controls, Frozen V4 and contiguous RBC Flat.
- Whole-trajectory repeats: 4; warmup: 8 frames, method order alternated by trajectory.
- Runner reported successful source receipt checks and **full native current-pair identity** with the reference for all 36 source scenes.
- **Direct paired query ratio**: frozen V4 mean time / RBC Flat mean time, scene-level geometric mean **1.05243x**. Above 1 favors Flat.
- **Flat scene wins versus V4**: **22/36**.
- Arithmetic reduction implied by inverse ratio: approximately **4.98%** lower time under the aggregate ratio interpretation. This is not a confidence bound or whole-solver performance result.
- Source directory: `output/rbc-final-run01-controlled-01/`, containing `RBC_FINAL_REPEATED_RESULTS.md`, `RBC_FINAL_REPEATED_RESULTS.json`, raw CSVs and logs on the user's Mac.

## Guard A/A quality control from the printed log

The 36 rounded per-scene `Guard A / Guard B` ratios in exact log order:

```text
1.0607 0.9921 0.9907 1.0108 0.9785 1.0117
0.9826 0.9944 1.0041 1.0563 1.0094 1.1561
0.9833 0.9976 1.0300 0.9604 1.0155 1.0081
1.0096 0.9910 0.9985 1.0483 1.0226 1.0027
1.0086 1.0019 0.9409 0.9977 1.1109 0.9979
1.0085 0.9230 1.0919 0.9707 1.0051 0.8548
```

Computed from **four-decimal rounded transcript values**, the A/A geometric mean is approximately **1.00507x**; the median is about **1.0034x**; the observed ratio range is **0.8548–1.1561x**; **8/36** individual scenes have an absolute deviation from 1 greater than 5%.

These ratios are a necessary **warning about per-scene timing variability**. They are not a variance model or confidence interval for the RBC-vs-V4 effect. Four repeats of identical existing scenes do not increase the independent scene count to 144. The native ordering controls are only alternating AB/BA at the whole-trajectory level, not separate thermal-session blocked randomization.

## Research decision

1. Do **not** promote a universal Flat win, SOTA novelty, credible statistical significance or full-solver benefit from Run 1.
2. Retain **Frozen V4 as the current validated engineering release**, and retain RBC Flat as an experimental candidate.
3. Preserve this Run-1 negative/control evidence, including adverse A/A scenes; do not rerun to remove them.
4. The next native Mac batch is **Run 2 / 5**, preregistered untouched seeds and multi-regime generalization. No tuning based on later holdout.
5. Presentation/paper revision **does not consume a new native Mac run**.

## Provenance limitation

This document records the user's terminal log and prior repository evidence. The original Mac Run-1 JSON, CSV timing traces and SHA manifest were **not separately uploaded into this conversation**, so all computed A/A summary statistics here are derived from the *rounded log*, not an independent reanalysis of original source bytes. Before submission, archive the exact native outputs and calculate uncertainty using the raw per-scene/repetition traces.

See `FINAL_FIVE_RUN_CONTRACT.md` for publication acceptance gates and the final four-run plan.
