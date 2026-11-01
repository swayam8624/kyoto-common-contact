# Common broad-phase research demo in 3D

Launch the native, offline demo with `./open_3d_demo.sh`. It opens the self-contained Blender scene using a clean startup configuration. No localhost server is needed.

Use the View Layer selector to switch among **01 Physics replay**, **02 Candidates · frame 60**, and **03 Proximity · frame 60**. Space plays the recorded physics; mouse navigation rotates the 3D scene. The two analysis stages are static snapshots of the same simulated frame.

The video `output/research3d/Common_Broad_Phase_3D.mp4` is a portable playback of the actual recorded cloth simulation. The native `.blend` remains fully three dimensional.

Blender computes cloth physics; the C++23 common-bound query processes exported, evaluated triangle geometry. The render highlights C++ candidate and precise-proximity classifications. The cutaway is a visualization only; queries use the full mesh. This does not yet replace the internal collision system of Blender, IPC, OGC or XPBD.

See `output/research3d/README.md` for regeneration, provenance and verification.

The earlier point-triangle HTML illustration is retained in the output directory as a historical diagnostic.
