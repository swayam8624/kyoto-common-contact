
## Passes 39C–39E — executed physical floor-contact evidence, independent reference and visualization

- [x] Recover and hash-verify ten actual headers at pinned source
  e27353d268cf99845dd361ae2a4723f975702bfd; use native Body::step
  with neo-Hookean elasticity and analytic floor projection.
- [x] Six 120-step physical trajectories per compiler (48/120/192
  tetrahedra, free and floor-impact); 7,205 nodal impacts per compiler,
  minimum endpoint J 0.89060718118, maximum absolute vertical momentum
  residual 8.21121e-13 N s.
- [x] Surface candidate replay: 72 distinct sampled steps, five
  ABBA/BAAB blocks each, excluded warmup, 1,440 timed queries per
  compiler. All 27,384 canonical candidate identities match exhaustive
  reference. Geomean exhaustive/SAP 1.652725 (GCC), 1.512898 (Clang).
  Candidate replay does not change the analytic floor integrator.
- [x] All 240 linear chords between committed states for 48-cell
  geometry independently certified with exact volume audit
  (84,334 candidate cell pairs, no unknown). Chord certification
  is not the actual intra-step piecewise floor-impact trajectory.
- [x] Independent NumPy/SciPy assembly and one-step replay for all
  720 steps; cKDTree candidate parity and free-motion DOP853
  comparison. Both compilers' independent CSV identical.
  Largest component force error 1.686e-12 N, position 1.111e-16 m,
  velocity 9.194e-16 m/s, impulse 2.665e-15 N s.
- [x] Generate fixed-camera actual recorded 3D stress animation,
  orientation and contact impulse figures and paired timing plot.
  Reproducible visualization code and evidence summaries committed.
  Raw local data archive retained separately; SHA-256 manifest names
  each raw file. Do not mistake the manifest for uploaded raw bytes.
- [ ] Pass 39C full coupled solver CPU and real Metal hardware.
- [ ] Pass 39D upstream IPC native backend and independent mesh-contact
  dynamics integration. SciPy is numerical validation only.
- [ ] Pass 39E coupled mesh-mesh accepted/rejected trajectory visualizations.

Reproduction with C++23, Boost, NumPy, SciPy, pandas, Matplotlib and ffmpeg:
    cmake -S . -B build-floor -DCMAKE_BUILD_TYPE=Release -DKYOTO_WITH_EXACT_SELF_VOLUME=ON
    cmake --build build-floor --target kyoto_pass39_native_floor_replay --parallel 2
    ./build-floor/kyoto_pass39_native_floor_replay evidence/gcc
    python3 tools/pass39_independent_replay.py evidence/gcc evidence/independent-gcc.csv
    python3 tools/pass39_visualize.py evidence/gcc visuals

Use separate compiler-specific build and evidence directories. Recorded
timings used strict C++23 -O2 without fast math in a shared Linux VM;
Release CMake timings can differ. No production whole-solver acceleration
or GPU performance conclusion is supported.
