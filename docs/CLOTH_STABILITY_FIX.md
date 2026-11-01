# Numerical stability correction for the multi-cloth physics fixture

## Reproduction before the change

The initial three-cloth implementation physically exploded despite passing candidate coverage and solver-state equivalence tests. A 260-frame Linux diagnostic observed maximum world coordinate 146.75, maximum edge stretch 867.09 times its rest length, and a maximum one-frame particle displacement of 158.92. This is real numerical instability, not merely exaggerated camera movement.

## Diagnosed mechanisms

The old wind forced particle positions by 0.003 meters every 1/90-second timestep (equivalent to about 24 m/s^2 lateral acceleration, far above what the author apparently intended). The three cloths were initially only 0.046 meters apart, already inside their 0.062-meter contact thickness. Hundreds of contacts could affect a shared vertex during the same solver iteration, and independently capped primitive-pair corrections could accumulate into arbitrarily large vertex motion. Finally, position-only collision corrections became spurious velocity at the next Verlet step. Contacts were able to displace vertices below the ground after the original floor constraint.

The old global guarded cache also passed conservative extra candidate pairs straight into approximate contact response. Cached pairs must be compared against CURRENT geometry before moving particles. This became an actual paired-physics consistency issue when the updated solver bounded per-vertex projections.

## Implemented physics-only corrections

- Wind is now a physically integrated acceleration: 1.6 m/s^2 peak times dt squared, not a constant position kick.
- Initial cloth-layer spacing is 0.19 meters, clear of initial contact thickness.
- Cloth anchor drives subtract their time-zero phase offsets, eliminating a large first-frame sheet-specific corner teleport.
- The total contact projection per dynamic particle is capped at 0.030 meters PER constraint iteration, including all primitive pairs sharing that vertex.
- 94% of collision and floor projection displacement is removed from next-frame Verlet velocity (approximately inelastic positional response), while genuine gravity, external animation and structural motion remain.
- Free velocity is damped by 0.982 per frame with a 0.060-meter-per-frame emergency cap.
- Ground constraints are enforced after contacts, not only before broad-phase queries.
- Global guarded candidate false positives are rejected using current padded AABBs before expensive geometric response. This extra filtering is measured, not free.
- Per-step world coordinate, frame displacement, grid-edge stretch and velocity-cap events are exposed to the native demo, which pauses on catastrophic instability.

This modification is to the common physical test fixture, NOT the online split/merge broad-phase policy. The same deterministic physical equations are run under all policies. The upstream broad-phase continues to run BEFORE the precise contact stage.

## Regression and results

The C++ stability probe under apps/cloth_stability_probe.cpp now fails if any complete multi-cloth run exhibits runaway coordinates, frame displacement above 0.8 m, structural edge strain above 2.6 times rest length, floor violation, missing real contacts, or missing actual dynamic motion. It is part of meeting preparation and CI, and reports the full frame-by-frame evidence.

The final 480-frame Linux stability run, after removing the sheet-specific first-frame anchor teleport, recorded max world coordinate **2.454** (versus 146.75 before), max edge stretch **1.150x** (versus 867x before), and max one-frame displacement **0.0832** (versus 158.92 before). First-frame movement was only **0.00364**, rather than 0.2426 in the first stabilized iteration. The cloths remained active, generating more than **1.7 million real inter-sheet positional contact corrections** over the run. A separate test checks physical equivalence across SAP, guarded SAP, fixed regional, and online split/merge policies.

Use scripts/prepare_meeting.sh and open_multicloth_demo.sh to rerun the checks and live demonstration. See output/native/cloth-stability.txt for local machine evidence. Regenerate all prior benchmark figures because the old and corrected physics scenarios are NOT directly comparable.

## Honest scope

This is still a discrete, approximate XPBD-style physics fixture: not a robust IPC/OGC/CCFR implementation, not an intersection-free/continuous-time-contact solver, and not material-calibrated cloth. Numerical stability, physical plausibility, guaranteed continuous collision avoidance, and collision-detection correctness are separate research properties. No claim of publication-level broad-phase novelty is implied by this stability repair.
