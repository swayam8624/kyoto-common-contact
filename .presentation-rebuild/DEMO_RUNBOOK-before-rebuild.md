# 4–5 minute live walkthrough

Start the app with `./run_demo.sh`, then open `http://127.0.0.1:4173`.

1. **Elastic collision (about 45 seconds).** Leave the default scene running. Point out the three sphere states and the six-stage panel. When the spheres meet, the C++ core creates a verified contact and applies an equal-mass, unit-restitution impulse. Pause and step once to show deterministic control.
2. **Brute force vs. sweep-and-prune (about 45 seconds).** Reset to frame 0, note the selected broad phase, then switch modes. The algorithm switch recomputes candidates without moving bodies or reapplying response. Both modes use the same AABB candidate semantics.
3. **Contact islands (about 45 seconds).** Choose “Separated contact islands”. The two clusters are colored by connected components built from verified sphere contacts; candidate-only edges do not join islands.
4. **Dense candidate reduction (about 60 seconds).** Choose “Dense candidate reduction”. Compare candidate and possible-pair counts in each mode. Candidate count is the broad-phase output; verified contacts are the sphere-distance subset. The pair ledger marks both. Treat the timing display as a local diagnostic, not a benchmark.
5. **Discrete vs. swept tunneling (about 60 seconds).** Choose “Tunneling check” and leave “Discrete” selected. Step once: the endpoint query reports no contact and the spheres exchange sides. Select “Swept / TOI” and reset the scene; the swept AABBs show the candidate before stepping. Step once: the C++ core records an impact time inside the fixed step, applies the elastic impulse, and returns motion paths that bend at impact. The dashed line shows the no-collision endpoint prediction.
6. **Capture and replay (about 30 seconds).** Choose “Start capture”; capture resets the selected scene to frame 0 and pauses. Step or resume, then choose “Stop capture” and “Replay”. JSON exports the visible frame plus the saved command sequence. “Load replay” accepts that JSON export and replays it through the C++ service after a page refresh. CSV exports the visible frame’s bodies and contacts.
7. **Close with the research boundary (about 30 seconds).** Swept bounds and analytic TOI are implemented for linearly translating spheres only. Mesh proximity, mesh rotation/deformation, benchmark campaigns, and adapters to IPC, OGC, XPBD, or CCFR are still open work. Per-frame timings remain diagnostics, not a benchmark campaign.

If the viewer stalls, refresh the tab. If the C++ service exits, stop `run_demo.sh` with Ctrl+C and launch it again. The app binds only to `127.0.0.1`.
