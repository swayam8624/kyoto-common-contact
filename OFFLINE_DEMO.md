# Offline common broad-phase demo

Build and generate the standalone page:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --target common_bounds_demo --parallel
./build/common_bounds_demo
```

Open `output/Common_Broad_Phase_Demo.html` directly in a browser. It has no server dependency. The C++ executable computes all geometry, bounds, candidate states, and exact distances before embedding the results in the HTML. The page only replays and draws those values.

## Shared record and margin

The single broad-phase input/output contract shown here is:

```cpp
PrimitiveBounds { id, kind, min[3], max[3] }
candidate pair { stableIdA, stableIdB }
```

An adapter for any future solver method could provide bounds in that record. No adapter or solver integration is implemented by this example. The caller supplies the proximity margin `delta`; the broad phase does not select it. Each primitive AABB is expanded by `delta / 2`, which conservatively retains all pairs within Euclidean distance `delta`; coordinatewise box overlap can also admit larger Euclidean separations. Equality on an AABB face counts as overlap.

The experiment uses triangle ID 2 with vertices `(-0.48,-0.42,0)`, `(0.48,-0.42,0)`, `(0,0.52,0)` and point ID 1 moving linearly from `(-1,0.08,0.08)` to `(1,0.08,0.08)` over normalized timestep `t ∈ [0,1]`. The margin is `delta = 0.12`. The endpoint union contains the entire point path because the motion is linear. Sweep-and-prune candidate pairs are compared with exhaustive AABB overlap, both for this interval and for twelve deterministic generated scenes. Every one of 81 sampled point-to-triangle distances is also checked: any distance `<= delta` must have a padded AABB candidate. The timeline reports instant-position candidates separately from the single candidate for the full motion interval.

This is a small correctness illustration, not a solver integration, performance benchmark, or proof for nonlinear motion, deformation, rotation, or other bound adapters.
