# Native research walkthrough

Run `./open_multicloth_demo.sh`. This is the primary live C++ experiment. The previous one-cloth, HTML and Blender artifacts are historical supporting material.

1. **Geometry:** press R to reset and F to frame the complete scene. Three separately colored sheets interact with two moving spheres. Space pauses; N advances one fixed physics step. The renderer reads the running C++ particle state.
2. **Shared boundary:** 2 displays region guard boxes; 3 displays candidates; 4 displays corrected contact pairs. Bounds and candidate IDs belong to broad phase. Precise feature checks and positional response belong to the common test solver.
3. **Online changes:** T enables/disables online control; Q cycles SAP/global/regional/online policies. G opens detailed statistics. Explain observed work → smoothed feedback → bounded split/merge/traversal decision → certificate invalidation. A split changes region membership, not cloth topology.
4. **Coverage:** A enables the independent periodic exhaustive AABB audit. Read “unchecked” literally: zero misses without an executed audit is not coverage evidence. Audit frames cost additional time.
5. **Interaction:** left drag visible fabric to attach a barycentric grab, right drag to orbit, S toggles self-contact. The headless suite tests moving-target attachment and a deliberate self-fold. The review's desktop input automation did not reliably attach to the rebuilt window, so its attempted drag captures are excluded from the slides; a human live-drag check remains outstanding.
6. **Results:** use the dated research deck and `output/review-2026-11-02/RESULTS.md`. In the three-cloth short comparison, online improves on SAP but global guarded SAP is faster. Show all methods. Describe ranges as ranges of repeat means, not confidence intervals.

Candidate counts describe the latest query. Contact counters count executed corrections across solver iterations; they are not counts of unique contacts. Controller decisions are made after a completed timestep and affect subsequent queries. Current guard overlays describe the completed query; a just-selected partition may first appear in the next step.

Full screenshot/capture commands, option semantics and current visual limits: `docs/NATIVE_CASES.md`. The native HUD fits the window; G contains the longer research log. Keep self-contact enabled and the full scene framed for the presentation. No CCD or nonpenetration guarantee is claimed.
