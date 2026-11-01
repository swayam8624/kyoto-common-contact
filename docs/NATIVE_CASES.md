# Reproducible native simulation cases

The native viewer draws the running C++ simulation directly. It does not replay a recorded mesh. Launch `./build/ccp_native.app/Contents/MacOS/ccp_native` from the repository root, or use `./open_multicloth_demo.sh` for the regular three-sheet online scene.

The native executable accepts:

| Option | Effect |
| --- | --- |
| `--multicloth` | Open the coupled cloth simulator. |
| `--cloth-count 1..3` | Select the number of live sheets; implies `--multicloth`. |
| `--sphere-count 1..2` | Select live moving spheres; implies `--multicloth`. |
| `--policy sap\|global\|regional\|online` | Select the actual coupled broad-phase policy. Default: `regional`. |
| `--online` | Shorthand for `--policy online`. |
| `--self-collision on|off` | Set same-sheet collision detection; default: on. |
| `--capture-after N` | Advance exactly N fixed simulation steps, pause, and capture the rendered state. |
| `--capture PATH` | Save the current OpenGL back framebuffer as a PPM image. Without `--capture-on-key`, capture occurs after render warm-up. |
| `--capture-on-key` | Keep simulation/rendering live and capture when F12 is pressed. Each trigger overwrites that configured path. Requires `--capture PATH`. |
| `--baseline` | Select rebuild-every-iteration for the single-cloth viewer; in multi-cloth mode, select SAP unless `--policy` or `--online` explicitly sets a policy. |
| `--trace PATH` | Write per-step single-cloth telemetry or per-step multi-cloth telemetry to CSV. |
| `--exit-after-capture` | Close after saving the requested capture. Requires `--capture PATH`. |
| `--stage 1..4` | Select geometry, bounds, candidates, or proximity/contact visualization. |
| `--paused` | Start paused; N advances a single step. |

Invalid options and out-of-range counts fail with an error. `--help` prints the current CLI synopsis. Startup prints the selected scene configuration and log locations. Multi-cloth CSV rows include the actual selected policy, sheet count, self-collision state, candidate/contact counts, self-contact/piercing counts, grab state, oracle checked/miss flags, solver timings and a position hash. `oracleMisses=0` is not a coverage result unless `oracleChecked=1`. Event logs contain physical frame and interaction state. These are diagnostic outputs, not independent correctness proofs.

## One-sheet SAP baseline

```sh
./build/ccp_native.app/Contents/MacOS/ccp_native \
  --cloth-count 1 --sphere-count 1 --policy sap --capture-after 240 \
  --capture /tmp/ccp-one-sheet-sap.ppm --trace /tmp/ccp-one-sheet-sap.csv \
  --exit-after-capture
```

This is one live cloth, one moving sphere, and a SAP rebuild policy. The simulation advances 240 fixed steps before pausing. The image is from the actual native OpenGL framebuffer. Since `--cloth-count` selects the coupled simulator even for one sheet, this CSV contains `MultiCloth` SAP diagnostics, not the original single-cloth viewer trace. `--baseline` also selects SAP here unless an explicit policy option overrides it.

## Three-sheet online case

```sh
./build/ccp_native.app/Contents/MacOS/ccp_native \
  --cloth-count 3 --sphere-count 2 --policy online --capture-after 480 \
  --capture /tmp/ccp-three-sheet-online.ppm \
  --trace /tmp/ccp-three-sheet-online.csv --exit-after-capture
```

This advances the real three-sheet/two-sphere solver for 480 frames using the online contact-aware partition controller. Self-collision starts enabled. Check the CSV's `selfCollision`, `selfContacts`, `selfPiercings`, `interContacts`, `sphereContacts`, and `policy` columns; the HUD also displays current counts. A zero self-contact count means the captured run does not demonstrate a self-contact event, even though self-collision was enabled. Use `oracleChecked` alongside `oracleMisses`; an unchecked row is not a successful oracle audit.

## Natural fold and self-contact evidence

There is no scripted fold fixture in the native viewer. In particular, the test-only patch displacement hook is not used to create a presentation scene. The following command provides a reproducible live one-sheet run and records whether its real motion produces contact:

```sh
./build/ccp_native.app/Contents/MacOS/ccp_native \
  --cloth-count 1 --sphere-count 1 --policy online --self-collision on \
  --capture-after 1200 --capture /tmp/ccp-one-sheet-self-contact.ppm \
  --trace /tmp/ccp-one-sheet-self-contact.csv --exit-after-capture
```

Only report a fold/self-contact example if the captured HUD or CSV shows a positive `selfContacts` or `selfPiercings` value. If neither becomes positive, the run is evidence of an enabled detector, not of a realized contact. For a deliberate real interaction, start the running three-sheet scene, use the left mouse button to pick and pull cloth, then inspect `selfContacts` and `selfPiercings`. The simulation uses the actual barycentric grab attachment and same-sheet collision path. These discrete checks do not establish continuous collision detection or an intersection-free guarantee.

## Actual mouse-drag capture

Mouse input is intentionally not synthesized by the CLI. Start a live scene, grab a visible cloth triangle and press F12 to capture the resulting native frame:

```sh
./build/ccp_native.app/Contents/MacOS/ccp_native \
  --cloth-count 3 --policy online --capture-on-key \
  --capture /tmp/ccp-live-drag.ppm --trace /tmp/ccp-live-drag.csv
```

Left-click cloth to begin the barycentric attachment; drag to move its target in the camera-facing plane; release to remove the grab. Gold/cyan markers show the selected point and target. Press F12 while the pull is visible. The CSV records `grab=1` and the solver's attachment error on each physical frame; the image is the actual OpenGL framebuffer captured on F12. A capture made after release will correctly show `grab=0`, so use the live HUD/event log if the image is intended to prove the interaction while held.

Right-drag or Shift+left-drag orbits the view. The picking ray uses GLFW window coordinates and the logical viewport, while rendering uses framebuffer dimensions; this distinction matters on Retina displays.

## Current visual verification boundary

The dated review bundle contains successful native fixed-step one-sheet SAP and three-sheet SAP/online framebuffers with per-step logs. The desktop input connector repeatedly returned `noWindowsAvailable` after relaunch, or no positive grab rows. Those attempts are retained as unverified diagnostics and are excluded from presentation evidence. Headless tests exercise ray picking, barycentric moving-target attachment and folded self-contact. A real user mouse-drag visual check remains outstanding.
