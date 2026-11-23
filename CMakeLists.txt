cmake_minimum_required(VERSION 3.20)
project(kyoto_spacetime_contact_atlas LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(kyoto_contact INTERFACE)
target_include_directories(kyoto_contact INTERFACE include)
if(MSVC)
 target_compile_options(kyoto_contact INTERFACE /W4)
else()
 target_compile_options(kyoto_contact INTERFACE -Wall -Wextra -Wpedantic)
endif()
enable_testing()

# Pass 5: actual 3D tetrahedral neo-Hookean volumetric dynamics,
# native rigid halfspace contact. This core does NOT depend on IPC.
add_executable(kyoto_solid_tests tests/solid_dynamics_tests.cpp)
target_link_libraries(kyoto_solid_tests PRIVATE kyoto_contact)
add_test(NAME three_dimensional_hyperelastic_fem_physics COMMAND kyoto_solid_tests)

# Pass 6: nonplanar analytic rigid-sphere swept vertex CCD + true FEM.
add_executable(kyoto_curved_contact_tests tests/curved_contact_tests.cpp)
target_link_libraries(kyoto_curved_contact_tests PRIVATE kyoto_contact)
add_test(NAME nonplanar_curved_sphere_contact_physics COMMAND kyoto_curved_contact_tests)

# The independent true triangle-sphere distance audit is critical:
# vertex-safe does not imply surface-safe under a curved collider.
add_executable(kyoto_sphere_surface_tests tests/sphere_surface_audit_tests.cpp)
target_link_libraries(kyoto_sphere_surface_tests PRIVATE kyoto_contact)
add_test(NAME triangle_sphere_surface_clearance_audit COMMAND kyoto_sphere_surface_tests)

# Conservative continuous whole-face/sphere prefix guard, independent
# of (and currently not applied to) the physical response integrator.
add_executable(kyoto_triangle_sweep_tests tests/triangle_sphere_sweep_tests.cpp)
target_link_libraries(kyoto_triangle_sweep_tests PRIVATE kyoto_contact)
add_test(NAME continuous_whole_triangle_sphere_sweep_certificate
         COMMAND kyoto_triangle_sweep_tests)

# Pass 7A: globally synchronized whole-face safe advancement with
# barycentric effective-mass velocity impulses (not general IPC solver).
add_executable(kyoto_whole_face_tests tests/whole_face_contact_tests.cpp)
target_link_libraries(kyoto_whole_face_tests PRIVATE kyoto_contact)
add_test(NAME whole_face_synchronized_fem_contact COMMAND kyoto_whole_face_tests)

# Pass 8: reject globally clipped proposals; recursively re-integrate
# every actual elapsed physical substep, with transactional rollback.
add_executable(kyoto_time_consistent_tests tests/time_consistent_contact_tests.cpp)
target_link_libraries(kyoto_time_consistent_tests PRIVATE kyoto_contact)
add_test(NAME time_consistent_whole_face_adaptive_fem
         COMMAND kyoto_time_consistent_tests)

# Pass 9: mathematically independent six-endpoint swept-hull
# separation and a second finite-triangle Voronoi distance implementation.
# Unlike Pass 8, this audit is NEVER used to advance the solver.
add_executable(kyoto_independent_contact_tests tests/independent_contact_audit_tests.cpp)
target_link_libraries(kyoto_independent_contact_tests PRIVATE kyoto_contact)
add_test(NAME independent_swept_prism_and_fem_contact_audit
         COMMAND kyoto_independent_contact_tests)

# Pass 9 contact law's independent local kinetic work, P and L tests:
# 1200 seeded 3D frame-rotated impulse cases, independent point triangle.
add_executable(kyoto_impulse_balance_tests tests/contact_impulse_balance_tests.cpp)
target_link_libraries(kyoto_impulse_balance_tests PRIVATE kyoto_contact)
add_test(NAME independent_barycentric_impulse_energy_and_momentum
         COMMAND kyoto_impulse_balance_tests)

# Pass 10: inward-never-authorized IEEE outward interval arithmetic for
# every original 3D FEM swept triangle/sphere path and fast event leaf.
# This check is independent of the production physics integrator.
add_executable(kyoto_rounded_prism_tests tests/rounded_prism_certificate_tests.cpp)
target_link_libraries(kyoto_rounded_prism_tests PRIVATE kyoto_contact)
# Preserve distinct, outward-bounded elementary arithmetic operations.
# Uncontrolled fused reassociation could invalidate nextafter enclosures.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_rounded_prism_tests
                         PRIVATE -ffp-contract=off -fno-fast-math)
endif()

add_test(NAME outward_IEEE754_interval_swept_sphere_certificate
         COMMAND kyoto_rounded_prism_tests)
# Pass 10: WHOLE 3D FEM discrete momentum ledger: gravity, damping,
# internal hyperelastic forces, true contact vectors and angular
# transport are independently accounted on every accepted substep.
add_executable(kyoto_whole_fem_balance_tests tests/whole_fem_balance_tests.cpp)
target_link_libraries(kyoto_whole_fem_balance_tests PRIVATE kyoto_contact)
add_test(NAME full_FEM_gravity_damping_contact_momentum_ledger
         COMMAND kyoto_whole_fem_balance_tests)

# Pass 11A: actual indexed finite triangle meshes with true 3D VF/EE
# AND segment-triangle interior piercing detection; moving continuous
# triangle/triangle clearance, plus real source FEM negative control.
add_executable(kyoto_triangle_mesh_tests tests/triangle_mesh_contact_tests.cpp)
target_link_libraries(kyoto_triangle_mesh_tests PRIVATE kyoto_contact)
add_test(NAME finite_triangle_mesh_VF_EE_piercing_and_continuous_contact
         COMMAND kyoto_triangle_mesh_tests)

# Pass 11B: TRUE physical FEM contact response on a finite indexed
# nonplanar triangular rigid obstacle, including full continuous
# triangle/triangle motion safety and adaptive real-time reintegration.
add_executable(kyoto_rigid_mesh_fem_tests tests/rigid_mesh_fem_contact_tests.cpp)
target_link_libraries(kyoto_rigid_mesh_fem_tests PRIVATE kyoto_contact)
add_test(NAME real_nonplanar_finite_rigid_mesh_FEM_contact
         COMMAND kyoto_rigid_mesh_fem_tests)
# Pass 11C: actual time-reintegrated fast physical collision with 72
# finite nonplanar rigid triangles, NOT merely a rollback negative test.
add_executable(kyoto_fast_mesh_impact_tests tests/rigid_mesh_fast_contact_tests.cpp)
target_link_libraries(kyoto_fast_mesh_impact_tests PRIVATE kyoto_contact)
add_test(NAME actual_adaptive_fast_finite_rigid_mesh_FEM_collision
         COMMAND kyoto_fast_mesh_impact_tests)







add_executable(kyoto_time_consistent_experiment apps/time_consistent_experiment.cpp)
target_link_libraries(kyoto_time_consistent_experiment PRIVATE kyoto_contact)


add_executable(kyoto_whole_face_fem_export apps/whole_face_fem_export.cpp)
target_link_libraries(kyoto_whole_face_fem_export PRIVATE kyoto_contact)




add_executable(kyoto_curved_fem_export apps/curved_fem_export.cpp)
target_link_libraries(kyoto_curved_fem_export PRIVATE kyoto_contact)

add_executable(kyoto_solid_simulation apps/solid_simulation_export.cpp)
target_link_libraries(kyoto_solid_simulation PRIVATE kyoto_contact)
add_executable(kyoto_contact_tests tests/contact_candidates_tests.cpp)
target_link_libraries(kyoto_contact_tests PRIVATE kyoto_contact)
add_test(NAME exact_swept_aabb_candidate_parity COMMAND kyoto_contact_tests)

# New mathematically distinct Stage-1 motion boundary and exporter.
add_executable(kyoto_motion_tests tests/motion_envelopes_tests.cpp)
target_link_libraries(kyoto_motion_tests PRIVATE kyoto_contact)
add_test(NAME conservative_cubic_path_enclosure COMMAND kyoto_motion_tests)
add_executable(kyoto_motion_export apps/motion_lab_export.cpp)
target_link_libraries(kyoto_motion_export PRIVATE kyoto_contact)

# Stage-2 exact per-slab and selective pair-level temporal refinement.
add_executable(kyoto_temporal_tests tests/temporal_refinement_tests.cpp)
target_link_libraries(kyoto_temporal_tests PRIVATE kyoto_contact)
add_test(NAME certified_time_slab_candidate_safety COMMAND kyoto_temporal_tests)
add_executable(kyoto_temporal_experiment apps/temporal_experiment.cpp)
target_link_libraries(kyoto_temporal_experiment PRIVATE kyoto_contact)

# Pass 3: mesh-aware IPC-style typed feature broad phase. The two independent
# paths must exactly agree before any solver or external adapter is enabled.
add_executable(kyoto_feature_tests tests/feature_broadphase_tests.cpp)
target_link_libraries(kyoto_feature_tests PRIVATE kyoto_contact)
add_test(NAME typed_mesh_feature_candidate_parity COMMAND kyoto_feature_tests)

# Independent 3D kinematic scene export (not solver integration).
add_executable(kyoto_feature_scene apps/feature_scene_export.cpp)
target_link_libraries(kyoto_feature_scene PRIVATE kyoto_contact)

# Opt-in real external IPC Toolkit LBVH; default build stays lightweight.
# The official source is pinned by commit SHA (release v1.6.1).
# IMPORTANT: the actual external CMake dependency has heavy transitive
# requirements. It is NOT a stand-in written inside this repository.
option(KYOTO_WITH_OFFICIAL_IPC "Build and validate pinned IPC Toolkit LBVH" OFF)
if(KYOTO_WITH_OFFICIAL_IPC)
  if(CMAKE_VERSION VERSION_LESS 3.24)
    message(FATAL_ERROR "Official IPC Toolkit integration needs CMake 3.24+")
  endif()
  include(FetchContent)
  set(IPC_TOOLKIT_BUILD_TESTS OFF CACHE BOOL "Disable upstream test build" FORCE)
  set(IPC_TOOLKIT_BUILD_PYTHON OFF CACHE BOOL "Disable upstream Python" FORCE)
  set(IPC_TOOLKIT_WITH_CUDA OFF CACHE BOOL "CPU baseline for fairness" FORCE)
  set(IPC_TOOLKIT_WITH_MESHFEM_SPARSE OFF CACHE BOOL "No solver Hessians needed" FORCE)
  FetchContent_Declare(
    kyoto_official_ipc_toolkit
    GIT_REPOSITORY https://github.com/ipc-sim/ipc-toolkit.git
    GIT_TAG 19bcab7e1f5a0d2addfcd28513422ff248ea504e
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(kyoto_official_ipc_toolkit)
  add_executable(kyoto_official_ipc_lbvh_tests tests/ipc_lbvh_parity_tests.cpp)
  target_link_libraries(kyoto_official_ipc_lbvh_tests PRIVATE kyoto_contact ipc::toolkit)
  add_test(NAME pinned_official_ipc_lbvh_feature_coverage COMMAND kyoto_official_ipc_lbvh_tests)
  # Stage 4: real upstream continuous CCD, not merely LBVH candidate parity.
  add_executable(kyoto_official_ipc_ccd_tests tests/ipc_continuous_ccd_tests.cpp)
  target_link_libraries(kyoto_official_ipc_ccd_tests PRIVATE kyoto_contact ipc::toolkit)
  add_test(NAME pinned_official_ipc_continuous_CCD_coverage COMMAND kyoto_official_ipc_ccd_tests)

  # Pass 15: matched original FEM feature snapshots against actual
  # pinned official IPC Toolkit LBVH and a maintained refitted tree.
  # Candidate coverage only; the solver itself remains independent.
  add_executable(kyoto_pass15_official_ipc_fem_benchmark
                 apps/pass15_official_ipc_fem_benchmark.cpp)
  target_link_libraries(kyoto_pass15_official_ipc_fem_benchmark
                        PRIVATE kyoto_contact ipc::toolkit)
  # Pass 18: exhaustive real upstream CCD over EVERY cross-object
  # VF/EE stencil on original accepted fast finite-mesh FEM leaves.
  add_executable(kyoto_pass18_real_leaf_ccd_audit
                 apps/pass18_real_leaf_ccd_audit.cpp)
  target_link_libraries(kyoto_pass18_real_leaf_ccd_audit
                        PRIVATE kyoto_contact ipc::toolkit)
  add_test(NAME pinned_actual_fast_FEM_leaf_exhaustive_IPC_CCD_events
           COMMAND kyoto_pass18_real_leaf_ccd_audit
                   ${CMAKE_CURRENT_BINARY_DIR}/pass18-leaf-ccd)

  # Pass 5: official upstream CCD on genuine solver-integrated
  # tetrahedral trajectories, reporting unresolved self-contact.
  add_executable(kyoto_official_ipc_solid_audit tests/ipc_solid_trajectory_audit.cpp)
  target_link_libraries(kyoto_official_ipc_solid_audit PRIVATE kyoto_contact ipc::toolkit)
  add_test(NAME real_hyperelastic_solver_ipc_geometry_audit
           COMMAND kyoto_official_ipc_solid_audit ${CMAKE_CURRENT_BINARY_DIR}/ipc-solid-audit)


endif()

# Pass 13: native physically integrated FEM trajectory benchmark.
# Independent exhaustive and rebuilt sweep-and-prune reference only.
# This is deliberately NOT labeled maintained SCA or official IPC speedup.
add_executable(kyoto_pass13_reference_fem_benchmark
               apps/pass13_reference_fem_benchmark.cpp)
target_link_libraries(kyoto_pass13_reference_fem_benchmark PRIVATE kyoto_contact)
add_test(NAME genuine_FEM_rebuilt_SAP_vs_exhaustive_parity
         COMMAND kyoto_pass13_reference_fem_benchmark
                 ${CMAKE_CURRENT_BINARY_DIR}/pass13-fem-reference)

# Pass 14: actual maintained/refittable node hierarchy with dual-tree VF
# and self-tree EE traversal. This is broadphase candidate parity only.
add_executable(kyoto_refit_feature_hierarchy_tests
               tests/refit_feature_hierarchy_tests.cpp)
target_link_libraries(kyoto_refit_feature_hierarchy_tests PRIVATE kyoto_contact)
add_test(NAME maintained_refit_hierarchy_VF_EE_oracle_and_invalidation
         COMMAND kyoto_refit_feature_hierarchy_tests)

# Pass 15: source motion adapter is shared between local and pinned IPC
# measurement pipelines; verify physically integrated endpoint geometry.
add_executable(kyoto_fem_feature_snapshots_tests
               tests/fem_feature_snapshots_tests.cpp)
target_link_libraries(kyoto_fem_feature_snapshots_tests PRIVATE kyoto_contact)
add_test(NAME identical_real_FEM_feature_source_and_refit_contract
         COMMAND kyoto_fem_feature_snapshots_tests)

# Pass 17: stress the actual persisted VF/EE node-pair cut against
# independent exhaustive enumeration in dense and rapidly changing 3D.
add_executable(kyoto_temporal_frontier_stress_tests
               tests/temporal_frontier_stress_tests.cpp)
target_link_libraries(kyoto_temporal_frontier_stress_tests PRIVATE kyoto_contact)
add_test(NAME retained_spacetime_pair_frontier_dense_sparse_oracle
         COMMAND kyoto_temporal_frontier_stress_tests)

# Pass 19: reproducible 3D feature-level fuzzing across cached/uncached
# BVH, budgeted frontier, warm-order SAP and independent rebuilt SAP.
add_executable(kyoto_maintained_broadphase_fuzz_tests
               tests/maintained_broadphase_fuzz_tests.cpp)
target_link_libraries(kyoto_maintained_broadphase_fuzz_tests PRIVATE kyoto_contact)
add_test(NAME independent_seeded_3D_feature_broadphase_fuzz
         COMMAND kyoto_maintained_broadphase_fuzz_tests)

# Pass 20: selective AABB leaf/ancestor refitting. The ordinary full
# refit implementation is retained as an independent control.
add_executable(kyoto_selective_refit_tests
               tests/selective_refit_tests.cpp)
target_link_libraries(kyoto_selective_refit_tests PRIVATE kyoto_contact)
add_test(NAME selective_3D_feature_refit_exhaustive_oracle_and_rollback
         COMMAND kyoto_selective_refit_tests)

# Pass 21: two actual separately integrated neo-Hookean solids against
# one finite rigid triangle obstacle; all shared-space VF/EE candidates
# must match independent exhaustive, SAP and selective BVH oracles.
# It does NOT test coupled deformable/deformable collision response.
add_executable(kyoto_uncoupled_multibody_fem_tests
               tests/uncoupled_multibody_fem_tests.cpp)
target_link_libraries(kyoto_uncoupled_multibody_fem_tests PRIVATE kyoto_contact)
add_test(NAME real_two_body_FEM_uncoupled_shared_feature_oracle
         COMMAND kyoto_uncoupled_multibody_fem_tests)

# Experimental Pass 22A only: synchronized moving triangle/triangle
# conservative continuous separation certificate. No response forces.
add_executable(kyoto_moving_moving_triangle_certificate_tests
               tests/moving_moving_triangle_certificate_tests.cpp)
target_link_libraries(kyoto_moving_moving_triangle_certificate_tests PRIVATE kyoto_contact)
add_test(NAME conservative_moving_moving_finite_triangle_time_guard
         COMMAND kyoto_moving_moving_triangle_certificate_tests)
