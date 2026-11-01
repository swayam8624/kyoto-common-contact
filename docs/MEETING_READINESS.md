# Current meeting entry point — 2 November 2026

The primary experiment is `./open_multicloth_demo.sh`: live C++ multi-cloth dynamics, precise inter-sheet/self/sphere response, and online collision-region control. It is an independent XPBD-style test fixture, not an official IPC/OGC/CCFR adapter.

Use the repository README for build instructions and `DEMO_RUNBOOK.md` for the demonstration. Run `./scripts/prepare_meeting.sh` to regenerate standard evidence; the script now fails on an unsuccessful piped benchmark. Dated review measurements are kept separately under `output/review-2026-11-02/`, with source/build hashes and exact verification commands.

Use fresh results only. The old 2.351/0.999 ms single-cloth figures and results from unstable physics are historical. Isolated broad-phase timings, complete simulation-step timings, and rendering FPS must be presented separately. Multi-cloth CPU step timings include periodic oracle checks; the graphics renderer is outside that timer.

The review verified seven Release test suites, three supported sanitizer suites, 480-frame stability, three repetitions of all 21 policy/sheet combinations, three online traces, actual Bullet comparisons, and native fixed-step framebuffers. See the dated `VERIFICATION.md` for executed commands, test limits and the outstanding live mouse-drag check.

The proposal should state both outcomes: online adaptation executes real structural changes and matches tested SAP trajectories, but global guarded SAP is faster on the short three-cloth fixture. No universal speedup, publication novelty, calibrated physical accuracy or continuous collision guarantee has been established.

The older rigid-sphere contact-island/analytic TOI browser demo, static HTML illustration and Blender replay remain separate supporting experiments. They do not demonstrate cloth CCD or an external solver integration.
