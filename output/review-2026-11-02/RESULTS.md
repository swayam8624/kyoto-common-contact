# Fresh local research measurements — 2 November 2026

Raw source: `output/review-2026-11-02/after/`. Three local repetitions; Apple M2 Pro, Release build. Complete CPU step includes periodic exhaustive oracle; rendering excluded. Seven-policy comparisons advance 60 steps, exclude first eight; the controller trace advances 160, excludes first twelve for averages. Ranges below are ranges of trial means, not confidence intervals.

| Sheets | Policy | Mean CPU step (ms) | Trial-mean range (ms) |
|---|---|---:|---:|
| 1 | sap_rebuild | 0.793 | 0.709–0.913 |
| 1 | global_guarded | 0.544 | 0.533–0.552 |
| 1 | region_fixed | 0.691 | 0.684–0.696 |
| 1 | region_adaptive | 0.679 | 0.662–0.695 |
| 1 | region_sweep_fixed | 0.729 | 0.706–0.770 |
| 1 | region_sweep_adaptive | 0.729 | 0.681–0.824 |
| 1 | online_contact_work | 0.726 | 0.715–0.732 |
| 2 | sap_rebuild | 4.507 | 4.396–4.723 |
| 2 | global_guarded | 3.806 | 3.696–3.995 |
| 2 | region_fixed | 4.272 | 4.072–4.636 |
| 2 | region_adaptive | 4.265 | 4.062–4.611 |
| 2 | region_sweep_fixed | 4.180 | 4.102–4.292 |
| 2 | region_sweep_adaptive | 4.095 | 4.074–4.129 |
| 2 | online_contact_work | 4.107 | 4.082–4.147 |
| 3 | sap_rebuild | 9.670 | 9.615–9.777 |
| 3 | global_guarded | 8.289 | 8.245–8.315 |
| 3 | region_fixed | 8.797 | 8.619–8.928 |
| 3 | region_adaptive | 8.696 | 8.613–8.816 |
| 3 | region_sweep_fixed | 8.868 | 8.698–9.044 |
| 3 | region_sweep_adaptive | 8.656 | 8.493–8.809 |
| 3 | online_contact_work | 8.902 | 8.714–9.025 |

All 63 coupled result rows report maximum position error 0 against SAP. The online trace separately compares every frame and periodically audits candidate coverage.

[
  {
    "trial": 1,
    "splits": 12,
    "merges": 0,
    "switches": 4,
    "auditedFrames": 11,
    "maxError": 0.0,
    "onlineMean": 12.57239077027027,
    "sapMean": 13.756218452702702
  },
  {
    "trial": 2,
    "splits": 12,
    "merges": 0,
    "switches": 4,
    "auditedFrames": 11,
    "maxError": 0.0,
    "onlineMean": 12.299589493243243,
    "sapMean": 13.417994094594594
  },
  {
    "trial": 3,
    "splits": 12,
    "merges": 0,
    "switches": 4,
    "auditedFrames": 11,
    "maxError": 0.0,
    "onlineMean": 13.495508202702702,
    "sapMean": 14.847087810810812
  }
]

These are one small deterministic family of fixtures, not independent scene samples. Timing-guided strategy decisions can differ between repetitions. No generic speedup, physical accuracy or collision-free guarantee follows.
The 480-frame stability probe reports damping/velocity clamps as well as geometric bounds; these stabilizing interventions must be disclosed.
