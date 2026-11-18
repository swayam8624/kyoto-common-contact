# Phase 9 — 25 real Khronos glTF meshes, exact CPU/Metal replay and readable solid-geometry viewer

## Verified evidence, not extrapolation

On 18 November 2026, the Kyoto Common Broadphase branch acquired **26 genuine GLB assets** (~63.4 MB) from a pinned Khronos glTF-Sample-Assets commit. The downloader verified per-model README license terms, recorded original source URIs and local SHA-256 hashes, and saved its pinned revision in ASSET_CATALOG.json. **25 objects were supported by the static-triangle importer**. Fox was deliberately rejected because its skin requires a skeletal pose which the importer does not implement.

The 25 admitted assets together contain **1,225,983 original triangles**. The first real 48-frame wave cohort, layout seed 66217, measured collision queries on **4,328 actually imported triangle AABBs** (max 256 triangles per model). The first cohort used approximately 0.35% of the original triangle count, so this is NOT a full-resolution collision accuracy benchmark. An updated display system independently renders up to 2,048 real source triangles per asset, while keeping measured collision triangles explicit and switchable in the UI. Latest CI displayed 30,110 actual source triangles. The display sample is never counted in candidate-pair timings.

The source meshes are transformed using deterministic prescribed rigid motion; the importer does not evaluate skinned animation, original textures/PBR shaders, collision forces or deformable cloth physics. The WebGL presentation is real source triangle geometry rather than arbitrary bounding boxes, but it is an inspectable replay, not a full physically coupled scene.

## Exact pair results on the updated 48-frame real wave

Successful measured Linux CI: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44270088284

| Method | Mean CPU query ms | Relative to frozen V4 |
|---|---:|---:|
| Instance shared BVH | **1.029244** | **1.797x** |
| Rebuilt SAP | 1.599307 | 1.156x |
| Instance X-sweep | 1.697242 | 1.090x |
| Instance exhaustive | 1.733563 | 1.067x |
| Frozen V4 | 1.849319 | 1.000x |
| V4-regional portfolio | 2.098823 | 0.881x |

Means exclude the first eight frames. All six CPU methods matched the exact sorted candidate-pair set on every frame. Approximately **56,549 cross-asset AABB candidate occurrences** were recorded in this particular updated 48-frame input, with 3,586 cross-asset pairs on its final frame. A candidate AABB overlap is not a confirmed triangle collision. These are exploratory single-run Linux timings; no statistical generalization or Apple Metal timings are implied.

The results again show why the current portfolio is not a publishable general controller: a fixed shared BVH outperforms V4, while the manager does not capture the fixed algorithm's advantage. Another workload may favor X-sweep. This motivates evaluating true per-region amortized maintenance, not cherry-picking whichever backend won after seeing the results.

## Larger independent experiment

A separate GitHub Actions workflow runs a **120-frame** genuine GLB cohort at up to **1,024 measured collision triangles per asset** and up to **3,072 real displayed triangles per asset**. Its seed 94681 is wired to real 3D object motion, not just a metadata label. The workflow is: .github/workflows/cbp-gltf-long.yml. This cohort has a strict full-pair exactness gate, a maximum 16-million-pairs/frame safeguard, and immutable evidence artifacts. It is NOT a million-scene run.

The earlier Phase 8 one-million-object benchmark used a **separate synthetic eight-AABB-per-island setup**, NOT one million imported models or soft-body simulations. Its 7.788 ms query result must never be used as evidence of one million deforming objects. The billion-AABB mode is strictly a capacity estimate: 24 GB of AABB storage per frame and 2.88 TB for 120 frames, excluding everything else; it has NOT processed a billion objects.

## Reproduce full real-model CPU and Apple Metal execution on macOS

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu \
  --target ccp_cbp_mass_replay ccp_cbp_contact_hotspots ccp_scaling_metal --parallel 4
python3 scripts/run_cbp_gltf_real_suite.py \
  --download-assets \
  --assets output/cbp-khronos-originals \
  --output output/cbp-25-real-scenes-wave-mac \
  --build build/kyoto-final-cpu \
  --frames 120 --layout wave --seed 94681 \
  --max-triangles-per-asset 1024 \
  --max-visual-triangles-per-asset 3072 \
  --gpu --open-preview --open-report --timeout 1500
```

The driver does not overwrite outputs. It downloads originals only once if the asset directory is absent. CPU methods, three real Apple Metal implementations, spatial intersection receipts and the self-contained real-mesh 3D visualization all use the same saved source triangle transforms and Float32 AABBs; full GPU/CPU candidate hashes must match. Its optional live preview shows genuine GLB surface triangles, not full real-time contact simulation.

For a separate crowded stress case, rerun the driver with a **new** --output directory, the same --assets directory, --layout crowded and an independent --seed. This changes geometric interaction conditions and allows comparison with wave. Do not rerun the earlier CSV into the same output directory.
