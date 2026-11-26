# Pass 63 — Experimental visualizations

Run `python tools/pass63_figures.py inputs figures` using the inputs directory from the Pass 63 evidence ZIP. Dependencies: numpy 2.3.5, pandas 2.2.3, matplotlib 3.10.8. The downloadable ZIP contains the exact input tables, accepted physical trajectory and receipts, renderer, 24 exports, offline gallery, and per-file SHA-256 manifest. The binary exports are not committed to this branch; use the archive or regenerate them. See pass63_manifest.json for the archive digest.

Input provenance: Pass47 original native artifact 15087213071; Pass59 stock IPC artifact 15100385729; Pass61 GCC 15101505043, Clang 15100079563 and stock IPC 15100748655. Pass62 supplies statistics and ablation summary. Pass58 supplies RSS audit and 156-cell baseline trajectory. Pass61 supplies the held-out physical summary.

Figures 1–8 are measured query statistics, witness reuse and fresh-process memory tradeoffs. Figures 9–11 are actual accepted endpoints and constraint counts, not pressure or penetration. Figure 12 shows trial subdivisions, not failed physical macrosteps. No new solver trials are claimed in this pass. Hardware Metal remains pending.
