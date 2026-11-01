# Verification record — 2 November 2026

Working directory: `/Users/swayamsingal/Desktop/Programming/kyoto-common-contact`.
Base commit: `4063f482` (see metadata for full ID). Checks were performed before the delivery commit; commit and push status are recorded in Git history. The pre-existing untracked `docs/MEETING_RESULTS.md` and `output/native/cloth-stability.txt` were preserved.

## Changes and diagnosis

- Regional cache padding could overflow from finite raw inputs. Padded bounds and guard envelopes now require finite results before certificates are used/committed. An oversized-margin rejection/recovery regression verifies the failure path.
- Removed a redundant output `unique` pass whose uniqueness follows from disjoint region membership and unique block construction; an explicit multi-region duplicate-pair regression checks that invariant. Removed unused contact helpers/locals/field. Removed a redundant native link dependency already supplied transitively.
- `prepare_meeting.sh` used `tee` pipelines that could hide failed simulations. It now uses Bash `set -euo pipefail`; malformed/incomplete evidence is rejected by the summarizers. A stdlib fixture suite is registered in CTest when Python is available.
- Native CLI now validates scene/policy/count/capture options; capture output checks open/write failures; CoreText allocation failures are checked. Picking rejects nonfinite inputs and handles a vertical camera basis. Tests cover those paths.
- Native multi-cloth traces now log the actual multi-cloth state at every physical step, including scene/policy, self-contact, grab state/error, oracle checked/miss flags and timings. The HUD fits its window and reports the actual selected traversal mode. Nonfinite/severe instability pauses the viewer; batch capture aborts after a detected instability.
- The online trace summary printed a literal `maxParticleError=0`. It now reports the computed maximum. The per-frame CSV already contained computed values; a fresh 160-step trace confirms the reporting path.
- README, runbook and meeting entry point now identify the current multi-cloth experiment. Algorithm, metric-scope, invocation and native-case documentation accompany the implementation.

## Executed commands and actual results

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

PASS: seven registered suites in the final full run (`tests-delivery.log`): evidence CSV fixtures, moving multi-cloth regional coverage, actual Bullet coverage, single-cloth contracts, legacy core contracts, common bounds, and mesh bounds. The CSV suite contains six unittest groups. Build logs are retained. No repository-wide formatter or additional static-analysis configuration was found; compiler checks, diff whitespace checks, and sanitizers were used.

```sh
cmake -S . -B /tmp/ccp-review-sanitized -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build /tmp/ccp-review-sanitized --target ccp_multicloth_tests ccp_live_tests ccp_tests --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ccp-review-sanitized \
  -R 'moving_multicloth_regional_coverage|live_cloth_contracts|ccp_core_contracts' --output-on-failure
```

PASS: three supported AddressSanitizer/UndefinedBehaviorSanitizer suites; 459.82 seconds. The first attempt with `ASAN_OPTIONS=detect_leaks=1` aborted before test execution because leak detection is unsupported by this macOS runtime. That failed attempt is retained in `sanitize-tests.log`; the supported rerun is `sanitize-tests-supported.log`. No leak-check success is claimed.

```sh
./build/ccp_cloth_stability_probe 480
./build/ccp_broadphase_leaderboard output/review-2026-11-02/after/isolated.csv
# Repeated three times with trial-specific CSV/log names:
./build/ccp_multicloth_benchmark output/review-2026-11-02/after/multicloth-1.csv 60
./build/ccp_online_trace output/review-2026-11-02/after/online-1.csv 160
./build/ccp_live_benchmark output/review-2026-11-02/after/single.csv
./build/ccp_patch_sweep output/review-2026-11-02/after/patch.csv 48
```

PASS: 480-step stability; all nine isolated algorithms over six workloads (54 rows, no required-pair misses); all seven coupled policies over 1/2/3 sheets in all three repeats (63 rows, reported maximum position error 0); three 160-step online traces (12 splits, zero observed merges, four switches and 11 audited frames each). Single-cloth benchmark and 26-row patch sweep also completed. Every current summarizer was run successfully on the fresh data, writing reports/charts inside this dated bundle. See `RESULTS.md`, raw CSVs, logs and `after/metadata.json` for settings and timing boundaries.

The full `prepare_meeting.sh` command was not run as one pipeline; its relevant native build/test/stability/benchmark/summary stages were executed directly. Historical rigid-sphere replay export is outside the primary cloth evidence.

## Native windows and capture evidence

Executed the actual `build/ccp_native.app/Contents/MacOS/ccp_native` with:

- `--cloth-count 1 --sphere-count 1 --policy sap --capture-after 120`
- `--cloth-count 3 --sphere-count 2 --policy online --capture-after 160`
- `--cloth-count 3 --sphere-count 2 --policy sap --capture-after 160`

Each run used explicit `--capture`, `--trace`, and `--exit-after-capture` paths under `cases/`. PASS: OpenGL framebuffer PPM/PNG and actual per-step logs generated. The paired three-sheet CSVs each contain 160 rows and matching position checksums throughout. Full particle-coordinate comparison is independently done by the benchmarks. GPU log: Apple M2 Pro, OpenGL 4.1 Metal. Fixed-step captures pause a real same-process simulation; they do not replay saved geometry.

Seven CLI help/error checks are recorded in `native-cli-checks.json`: help succeeds; invalid cloth count, malformed/negative capture count, invalid self-contact option, capture-exit without output, and unknown option fail before opening a scene.

**Outstanding interaction limit:** read-only native window inspection succeeded, but desktop input automation became unreliable after rebuilt-app relaunches (`noWindowsAvailable`) or produced no positive grab rows. Attempted `drag*` traces/images are unverified diagnostics and excluded from presentation evidence. Headless tests exercise ray picking, a changing barycentric grab target and deliberate self-fold correction; no successful human-style live drag is claimed. No Linux/Windows build or native PowerPoint application check was executed during this audit.

## Presentation checks

The 17-slide local PPTX uses editable text/tables, typeset equation PNG and actual local scientific plots/native framebuffers. Package integrity, layout/heading fit, required native tables, Arial font policy and Artifact Tool import passed. The final exported PPTX was re-imported and all 17 slides rendered for inspection. These checks do not prove Microsoft PowerPoint application behavior. See the finalizer receipt in `.presentation-review-2026-11-02/validation-v3.json`.

## Interpretation limits

Current AABB coverage is a discrete geometric contract, not CCD or intersection-free cloth simulation. Velocity clamps and damping affect the approximate solver. The online method is not fastest in the short three-cloth comparison; global guard wins there. Three local repetitions of a small fixed-resolution scene family do not establish publication novelty, physical accuracy or cross-solver generalization. Timed multi-cloth steps include periodic oracle work and exclude rendering. The cleanup itself has no statistically established speedup claim.

## Native inspector data refresh

The generated CSV inputs used by the native G inspector were backed up under `before/native-evidence/`, then refreshed from this bundle: single benchmark, first coupled/online repeat, isolated benchmark and patch sweep. The inspector therefore shows a fresh individual run; the slide policy chart aggregates all three coupled repeats. The pre-existing untracked Markdown/stability text files remain untouched.

## Refined presentation delivery

The later layout refinement expands the deck to 20 slides, adds interface/counter/stage-cost explanations, and preserves the measurements. The current deck is `output/Online_Common_Broad_Phase_Research_Layout_Final.pptx`; its validation receipt is `.presentation-refine-2026-11-02/validation-final.json`. All 20 slides were re-imported and rendered; package/layout checks report no findings or warnings. The earlier 17-slide validation above is retained as historical verification.
