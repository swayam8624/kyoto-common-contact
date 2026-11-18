# CBP Research Observatory V2 — visual interpretation and reproduction

Status: an evidence viewer, **not** a 1,000-material physically coupled collision solver. The original `viewer/mass_stress.html` remains unchanged; `scripts/render_cbp_mass_dashboard.py` defaults to V2, while `--legacy-view` selects the old version.

## Why the original preview was confusing

The user's Apple Silicon 1,000-world contact-front log showed zero cross-instance candidate pairs through its sampled frame 110, and only **9 cross-instance pairs out of 166,632 total element-AABB candidates at frame 120**. Most candidates exist *within* an instance. Drawing many moving objects without this distinction suggests nonexistent material-material contact. The worlds are independently stepped elastic instances; no between-world contact forces are solved.

## Implemented V2 improvements

- **Frame-specific research story:** exact CPU candidate totals, number and fraction involving separate material instances, timing of first detected inter-instance candidate, and limitations of the dataset.
- **3D WebGL playback:** familiar orbit, zoom, speed, seek, and material-family filter plus top/front camera presets, reset, clicked-instance focus, and isolate-instance mode.
- **Click-to-inspect XY map:** choose an actual material instance. Display its material type, generator regime, primitive AABB count, and possible neighbors from *envelope overlaps*. Neighbors are distinctly labeled a conservative proxy, not verified primitive candidate pairs.
- **Synchronized candidate timeline:** total and inter-instance pair counts on separately marked scales; click to seek through the corresponding frame. Preview-only mode never displays invented counts or timings.
- **Separated CPU and Apple Metal benchmark comparisons:** CPU query mean, P95 and V4-relative speedup are displayed separately from Metal host elapsed time and GPU-only compute-kernel duration.
- **Selected-frame measured CPU winner:** never feed retrospective winners into the production controller, never invent an oracle improvement.
- **Self-contained offline HTML:** can be regenerated from the old stored geometry, ownership and CSVs without recompiling or repeating the 100/1,000-world run.
- **Preserved legacy dashboard:** use `--legacy-view` for comparison with previous figures.

## Re-open already measured 100 and 1,000 Worlds on macOS (no rerun)

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git pull --ff-only origin research/common-broadphase-orchestrator
for size in 100 1000; do
  case="output/cbp-phase7-mac-gpu-120/materials${size}-frames120-seed43129-contact_front"
  if [[ -d "$case" && ! -e "$case/dashboard-v2.html" ]]; then
    python3 scripts/render_cbp_mass_dashboard.py "$case" --html "$case/dashboard-v2.html"
  fi
  if [[ -f "$case/dashboard-v2.html" ]]; then open "$case/dashboard-v2.html"; fi
done
```

Alternative geometry-only preview from an already exported case: invoke `scripts/render_cbp_mass_dashboard.py CASE --preview-only --html CASE/geometry-v2.html` with a fresh output name.

## Visual stress layout choice

The **crowded** and **collision_wave** generator layouts create much more cross-instance AABB overlap activity than the contact-front run. Use those for spatial interaction demonstrations. They are still independently integrated Worlds without a shared contact solver. GPU/CPU pair parity is confirmed only for modes actually measured on the same saved Float32 input.

## Scientific limitations

The colored lines are per-instance *envelopes*, not triangle meshes, physical contacts or precisely located collision pairs. AABB candidates require narrow-phase checks and optional CCD. CPU instance-owned regional kernels and the Metal algorithms' contiguous-block region scheme are different implementations. Separate Metal host and GPU-only timings must not be conflated. Do not extrapolate these visualizations to SIGGRAPH-level novelty, stable folding, or full solver speedup.

## Validation

GitHub CI validates small independent AABB candidate oracles, generates preview/full V2 and original V1 dashboards, checks browser JS syntax using Node, and exports a measured 100-world crowded HTML example with raw material data.
