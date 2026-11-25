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

  # Pass 35A: actual external broadphase inside native physical response.
  add_executable(kyoto_official_ipc_native_backend_tests tests/official_ipc_native_backend_tests.cpp)
  target_link_libraries(kyoto_official_ipc_native_backend_tests PRIVATE kyoto_contact ipc::toolkit)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_official_ipc_native_backend_tests PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_official_ipc_native_backend_tests PRIVATE /fp:strict)
  endif()
  add_test(NAME pinned_IPC_LBVH_inside_native_typed_FEM
           COMMAND kyoto_official_ipc_native_backend_tests)

  # Pass 34A: external CCD audit of actual connected self-contact FEM.
  add_executable(kyoto_pass34_connected_ipc_audit apps/pass34_connected_ipc_audit.cpp)
  target_link_libraries(kyoto_pass34_connected_ipc_audit PRIVATE kyoto_contact ipc::toolkit)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_pass34_connected_ipc_audit PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_pass34_connected_ipc_audit PRIVATE /fp:strict)
  endif()
  add_test(NAME pinned_IPC_connected_FEM_accepted_and_rejected_CCD
           COMMAND kyoto_pass34_connected_ipc_audit
                   ${CMAKE_CURRENT_BINARY_DIR}/pass34-connected-ipc)

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

# Pass 23: isolated original physically integrated FEM ABBA. Four
# independent maintained hierarchies compare old selective dependency
# marks with an optional >=50%-motion dense linear envelope refresh.
# No official IPC or solver-step speedup inferred from this ablation.
add_executable(kyoto_pass23_dense_refit_abba
               apps/pass23_dense_refit_abba.cpp)
target_link_libraries(kyoto_pass23_dense_refit_abba PRIVATE kyoto_contact)
add_test(NAME selective_dense_motion_physical_FEM_ABBA_oracle
         COMMAND kyoto_pass23_dense_refit_abba
                 ${CMAKE_CURRENT_BINARY_DIR}/pass23-original-fem-abba)

# Pass 24: opt-in stage attribution of *actual* C++23
# original-source FEM broadphase candidate generation, without
# conflating profiler-instrumented clocks and uninstrumented IPC.
add_executable(kyoto_pass24_stage_profile
               apps/pass24_stage_profile.cpp)
target_link_libraries(kyoto_pass24_stage_profile PRIVATE kyoto_contact)

# Pass 25: physically integrated same-run ABBA comparison of optional
# largest-span two-child VF/EE traversal vs independent legacy
# four-child VF/left-first EE traversal, both maintaining identical
# validated 3D swept feature caches and exhaustive candidate contract.
add_executable(kyoto_pass25_traversal_abba
               apps/pass25_traversal_abba.cpp)
target_link_libraries(kyoto_pass25_traversal_abba PRIVATE kyoto_contact)

# Pass 26: optional conservative prechecked child-pair VF/EE traversal,
# measured against independent incumbent recursive overlap testing
# with identical original physically integrated FEM source geometry.
add_executable(kyoto_pass26_prechecked_abba
               apps/pass26_prechecked_abba.cpp)
target_link_libraries(kyoto_pass26_prechecked_abba PRIVATE kyoto_contact)

# Pass 27: native solver-facing linear endpoint adapter. The opt-in
# pinned official IPC continuous CCD gate also exercises this adapter.
add_executable(kyoto_linear_candidate_adapter_tests
               tests/linear_candidate_adapter_tests.cpp)
target_link_libraries(kyoto_linear_candidate_adapter_tests PRIVATE kyoto_contact)
add_test(NAME indexed_linear_solver_candidate_adapter_oracle
         COMMAND kyoto_linear_candidate_adapter_tests)

# Pass 28: actual FEM solver with an optional static finite-face hierarchy.
add_executable(kyoto_static_face_solver_parity_tests
               tests/static_face_solver_parity_tests.cpp)
target_link_libraries(kyoto_static_face_solver_parity_tests PRIVATE kyoto_contact)
add_test(NAME actual_FEM_static_face_broadphase_physical_parity
         COMMAND kyoto_static_face_solver_parity_tests)

# Pass 29A: local two-finite-mass normal contact law, not coupled FEM.
add_executable(kyoto_coupled_triangle_normal_impulse_tests
               tests/coupled_triangle_normal_impulse_tests.cpp)
target_link_libraries(kyoto_coupled_triangle_normal_impulse_tests PRIVATE kyoto_contact)
add_test(NAME two_finite_mass_triangle_normal_impulse_balance
         COMMAND kyoto_coupled_triangle_normal_impulse_tests)

# Pass 29B: coupled finite-mass neo-Hookean volumetric bodies.
add_executable(kyoto_coupled_fem_contact_tests
               tests/coupled_fem_contact_tests.cpp)
target_link_libraries(kyoto_coupled_fem_contact_tests PRIVATE kyoto_contact)
add_test(NAME synchronized_two_body_FEM_contact_and_rollback
         COMMAND kyoto_coupled_fem_contact_tests)

# Pass 30A: continuous orientation under the explicit IEEE FP model.
add_executable(kyoto_tet_orientation_certificate_tests
               tests/tet_orientation_certificate_tests.cpp)
target_link_libraries(kyoto_tet_orientation_certificate_tests PRIVATE kyoto_contact)
add_test(NAME outward_continuous_tet_orientation_certificate
         COMMAND kyoto_tet_orientation_certificate_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  foreach(target kyoto_tet_orientation_certificate_tests kyoto_coupled_fem_contact_tests)
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  endforeach()
elseif(MSVC)
  foreach(target kyoto_tet_orientation_certificate_tests kyoto_coupled_fem_contact_tests)
    target_compile_options(${target} PRIVATE /fp:strict)
  endforeach()
endif()

# Pass 31A: outward synchronized moving triangle hull separation.
add_executable(kyoto_rounded_relative_triangle_tests
               tests/rounded_relative_triangle_certificate_tests.cpp)
target_link_libraries(kyoto_rounded_relative_triangle_tests PRIVATE kyoto_contact)
add_test(NAME outward_relative_moving_triangle_surface_certificate
         COMMAND kyoto_rounded_relative_triangle_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_rounded_relative_triangle_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_rounded_relative_triangle_tests PRIVATE /fp:strict)
endif()

# Pass 31B: native two connected toroidal volumes and source export.
add_executable(kyoto_coupled_torus_physics_tests
               tests/coupled_torus_physics_tests.cpp)
target_link_libraries(kyoto_coupled_torus_physics_tests PRIVATE kyoto_contact)
add_test(NAME coupled_toroidal_volumes_physical_parity
         COMMAND kyoto_coupled_torus_physics_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_coupled_torus_physics_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_coupled_torus_physics_tests PRIVATE /fp:strict)
endif()

# Pass 32A: restricted same-body nonincident face contact.
add_executable(kyoto_nonincident_self_contact_tests
               tests/nonincident_self_contact_tests.cpp)
target_link_libraries(kyoto_nonincident_self_contact_tests PRIVATE kyoto_contact)
add_test(NAME restricted_nonincident_same_body_contact
         COMMAND kyoto_nonincident_self_contact_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_nonincident_self_contact_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_nonincident_self_contact_tests PRIVATE /fp:strict)
endif()

# Pass 32B: typed nonincident self audit beyond whole-face adjacency.
add_executable(kyoto_typed_self_contact_audit_tests
               tests/typed_self_contact_audit_tests.cpp)
target_link_libraries(kyoto_typed_self_contact_audit_tests PRIVATE kyoto_contact)
add_test(NAME typed_adjacent_face_self_contact_audit
         COMMAND kyoto_typed_self_contact_audit_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_typed_self_contact_audit_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_typed_self_contact_audit_tests PRIVATE /fp:strict)
endif()

# Pass 32C/33A: typed connected self-contact physical response.
add_executable(kyoto_typed_connected_self_contact_tests
               tests/typed_connected_self_contact_tests.cpp)
target_link_libraries(kyoto_typed_connected_self_contact_tests PRIVATE kyoto_contact)
add_test(NAME typed_connected_FEM_self_contact
         COMMAND kyoto_typed_connected_self_contact_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_typed_connected_self_contact_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_typed_connected_self_contact_tests PRIVATE /fp:strict)
endif()

# Pass 36A: actual physical warm ownership across macrosteps.
add_executable(kyoto_persistent_fem_context_tests tests/persistent_fem_context_tests.cpp)
target_link_libraries(kyoto_persistent_fem_context_tests PRIVATE kyoto_contact)
add_test(NAME persistent_typed_FEM_physical_parity COMMAND kyoto_persistent_fem_context_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_persistent_fem_context_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_persistent_fem_context_tests PRIVATE /fp:strict)
endif()

# Pass 36B: fully paid native whole-step timings (not a speed assertion).
add_executable(kyoto_pass36_paid_persistent_fem_abba apps/pass36_paid_persistent_fem_abba.cpp)
target_link_libraries(kyoto_pass36_paid_persistent_fem_abba PRIVATE kyoto_contact)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_pass36_paid_persistent_fem_abba PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_pass36_paid_persistent_fem_abba PRIVATE /fp:strict)
endif()

# Pass 37A: matched paid CPU candidate scaling on accepted native leaves.
add_executable(kyoto_pass37_cpu_physical_scaling apps/pass37_cpu_physical_scaling.cpp)
target_link_libraries(kyoto_pass37_cpu_physical_scaling PRIVATE kyoto_contact)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_pass37_cpu_physical_scaling PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_pass37_cpu_physical_scaling PRIVATE /fp:strict)
endif()
add_test(NAME physical_CPU_candidate_scaling_smoke
         COMMAND kyoto_pass37_cpu_physical_scaling ${CMAKE_CURRENT_BINARY_DIR}/pass37-smoke 1)

# Pass 37B: optional native Metal correctness and paid mask baseline.
if(APPLE)
  enable_language(OBJCXX)
  add_executable(kyoto_pass37_metal_physical_scaling apps/pass37_metal_physical_scaling.mm)
  set_target_properties(kyoto_pass37_metal_physical_scaling PROPERTIES
    OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED ON OBJCXX_EXTENSIONS OFF)
  target_link_libraries(kyoto_pass37_metal_physical_scaling PRIVATE kyoto_contact "-framework Foundation" "-framework Metal")
  target_compile_options(kyoto_pass37_metal_physical_scaling PRIVATE -fobjc-arc -ffp-contract=off -fno-fast-math)
  add_test(NAME physical_Metal_candidate_scaling_smoke
    COMMAND kyoto_pass37_metal_physical_scaling ${CMAKE_CURRENT_BINARY_DIR}/pass37-metal-smoke 1)
  set_tests_properties(physical_Metal_candidate_scaling_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

# Pass 38A: initial cross-body volume disjointness, independent kernel
# and native physical transaction regression.
add_executable(kyoto_initial_tet_separation_tests tests/initial_tet_separation_tests.cpp)
add_executable(kyoto_initial_volume_fem_tests tests/initial_volume_fem_tests.cpp)
add_executable(kyoto_initial_tet_exact_oracle_tests tests/initial_tet_exact_oracle_tests.cpp)
foreach(target kyoto_initial_tet_separation_tests kyoto_initial_volume_fem_tests kyoto_initial_tet_exact_oracle_tests)
  target_link_libraries(${target} PRIVATE kyoto_contact)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endforeach()
add_test(NAME outward_initial_tetrahedral_separation COMMAND kyoto_initial_tet_separation_tests)
add_test(NAME initial_volume_FEM_containment_rollback COMMAND kyoto_initial_volume_fem_tests)

add_test(NAME initial_tetrahedral_exact_integer_oracle COMMAND kyoto_initial_tet_exact_oracle_tests)

# Pass 38C: standalone complete volume-boundary topology validation.
add_executable(kyoto_volume_boundary_topology_tests tests/volume_boundary_topology_tests.cpp)
target_link_libraries(kyoto_volume_boundary_topology_tests PRIVATE kyoto_contact)
add_test(NAME complete_volume_boundary_topology COMMAND kyoto_volume_boundary_topology_tests)

# Pass 38D: standalone and actual native volume factory compatibility.
add_executable(kyoto_volume_boundary_fixture_tests tests/volume_boundary_fixture_tests.cpp)
add_executable(kyoto_native_volume_boundary_fixture_tests tests/native_volume_boundary_fixture_tests.cpp)
foreach(target kyoto_volume_boundary_fixture_tests kyoto_native_volume_boundary_fixture_tests)
  target_link_libraries(${target} PRIVATE kyoto_contact)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endforeach()
add_test(NAME standalone_volume_boundary_fixture_patterns COMMAND kyoto_volume_boundary_fixture_tests)
add_test(NAME actual_native_volume_boundary_factories COMMAND kyoto_native_volume_boundary_fixture_tests)

# Pass 38E: optional exact static reference; no Boost dependency in core.
option(KYOTO_WITH_EXACT_SELF_VOLUME "Build Boost.Multiprecision static volume oracle" OFF)
if(KYOTO_WITH_EXACT_SELF_VOLUME)
  find_package(Boost REQUIRED)
  add_executable(kyoto_exact_tet_intersection_tests tests/exact_tet_intersection_tests.cpp)
  add_executable(kyoto_native_exact_self_volume_tests tests/native_exact_self_volume_tests.cpp)
  add_executable(kyoto_exact_volume_fixture_tests tests/volume_boundary_fixture_tests.cpp)
  target_compile_definitions(kyoto_exact_volume_fixture_tests PRIVATE KYOTO_EXACT_FIXTURE_AUDIT)
  add_executable(kyoto_exact_swept_tet_certificate_tests tests/exact_swept_tet_certificate_tests.cpp)
  add_executable(kyoto_exact_swept_volume_fixture_tests tests/exact_swept_volume_fixture_tests.cpp)
  add_executable(kyoto_pass39_exact_volume_abba apps/pass39_exact_volume_abba.cpp)
  add_executable(kyoto_exact_factored_axis_tests tests/exact_factored_axis_tests.cpp)
  add_executable(kyoto_exact_constant_axis_tests tests/exact_constant_axis_tests.cpp)
  add_executable(kyoto_exact_axis_heldout_tests tests/exact_axis_heldout_tests.cpp)
  add_executable(kyoto_exact_axis_heldout_volume_tests tests/exact_axis_heldout_volume_tests.cpp)
  add_executable(kyoto_pass39_native_floor_replay apps/pass39_native_floor_replay.cpp)
  add_executable(kyoto_pass39_coupled_volume_campaign apps/pass39_coupled_volume_campaign.cpp)
  add_executable(kyoto_pass39b_factored_axis_abba apps/pass39b_factored_axis_abba.cpp)
  add_executable(kyoto_native_exact_swept_volume_tests tests/native_exact_swept_volume_tests.cpp)
  foreach(target kyoto_exact_tet_intersection_tests kyoto_native_exact_self_volume_tests kyoto_exact_volume_fixture_tests kyoto_exact_swept_tet_certificate_tests kyoto_exact_swept_volume_fixture_tests kyoto_native_exact_swept_volume_tests kyoto_pass39_exact_volume_abba kyoto_exact_factored_axis_tests kyoto_pass39b_factored_axis_abba kyoto_exact_constant_axis_tests kyoto_exact_axis_heldout_tests kyoto_exact_axis_heldout_volume_tests kyoto_pass39_native_floor_replay kyoto_pass39_coupled_volume_campaign)
    target_link_libraries(${target} PRIVATE kyoto_contact)
    target_include_directories(${target} PRIVATE ${Boost_INCLUDE_DIRS})
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
    elseif(MSVC)
      target_compile_options(${target} PRIVATE /fp:strict)
    endif()
  endforeach()
  add_test(NAME exact_static_tetrahedral_intersection COMMAND kyoto_exact_tet_intersection_tests)
  add_test(NAME exact_continuous_tetrahedral_separation COMMAND kyoto_exact_swept_tet_certificate_tests)
  add_test(NAME exact_factored_axis_coefficient_and_pair_parity COMMAND kyoto_exact_factored_axis_tests)
  add_test(NAME exact_constant_axis_polynomial_parity COMMAND kyoto_exact_constant_axis_tests)
  add_test(NAME exact_axis_heldout_pair_parity COMMAND kyoto_exact_axis_heldout_tests)
  add_test(NAME exact_axis_heldout_whole_volume_parity COMMAND kyoto_exact_axis_heldout_volume_tests)
  add_test(NAME exact_continuous_volume_fixture_patterns COMMAND kyoto_exact_swept_volume_fixture_tests)
  add_test(NAME exact_continuous_native_FEM_leaf_volume COMMAND kyoto_native_exact_swept_volume_tests)
  add_test(NAME exact_static_volume_fixture_patterns COMMAND kyoto_exact_volume_fixture_tests)
  add_test(NAME exact_static_native_FEM_endpoint_self_volume COMMAND kyoto_native_exact_self_volume_tests)
endif()

# Pass 38I: dependency-free swept cell broad-phase parity.
add_executable(kyoto_swept_cell_candidates_tests tests/swept_cell_candidates_tests.cpp)
target_link_libraries(kyoto_swept_cell_candidates_tests PRIVATE kyoto_contact)
add_test(NAME swept_cell_candidate_exhaustive_parity COMMAND kyoto_swept_cell_candidates_tests)

# Pass 39D: independent external superset normalization regression.
add_executable(kyoto_external_candidate_filter_tests tests/external_candidate_filter_tests.cpp)
target_link_libraries(kyoto_external_candidate_filter_tests PRIVATE kyoto_contact)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_external_candidate_filter_tests PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_external_candidate_filter_tests PRIVATE /fp:strict)
endif()
add_test(NAME external_candidate_superset_normalization COMMAND kyoto_external_candidate_filter_tests)

# Paid stronger-baseline physical replay, no extra dependencies.
add_executable(kyoto_pass39_strong_cpu_baselines apps/pass39_strong_cpu_baselines.cpp)
target_link_libraries(kyoto_pass39_strong_cpu_baselines PRIVATE kyoto_contact)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(kyoto_pass39_strong_cpu_baselines PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(kyoto_pass39_strong_cpu_baselines PRIVATE /fp:strict)
endif()
add_test(NAME physical_strong_cpu_baselines_smoke COMMAND kyoto_pass39_strong_cpu_baselines ${CMAKE_CURRENT_BINARY_DIR}/strong-baselines-smoke 1)
if(KYOTO_WITH_EXACT_SELF_VOLUME)
  add_test(NAME physical_coupled_volume_campaign_smoke COMMAND kyoto_pass39_coupled_volume_campaign 1)
endif()

# Direct independent narrow phase from the pinned IPC dependency.
if(KYOTO_WITH_OFFICIAL_IPC AND TARGET tight_inclusion::tight_inclusion)
  add_executable(kyoto_pass39_tight_inclusion_native apps/pass39_tight_inclusion_native.cpp)
  target_link_libraries(kyoto_pass39_tight_inclusion_native PRIVATE kyoto_contact tight_inclusion::tight_inclusion)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_pass39_tight_inclusion_native PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_pass39_tight_inclusion_native PRIVATE /fp:strict)
  endif()
  add_test(NAME direct_upstream_tight_inclusion_native_accepted_rejected COMMAND kyoto_pass39_tight_inclusion_native)
endif()

# Executed candidate and interval projection comparison, optional production modes.
add_executable(kyoto_cached_axis_sap_tests tests/cached_axis_sap_tests.cpp)
add_executable(kyoto_relative_scalar_projection_tests tests/relative_scalar_projection_tests.cpp)
add_executable(kyoto_pass39_cached_sap_campaign apps/pass39_cached_sap_campaign.cpp)
add_executable(kyoto_pass39_cached_sap_solver apps/pass39_cached_sap_solver.cpp)
add_executable(kyoto_pass39_projection_solver apps/pass39_projection_solver.cpp)
foreach(target kyoto_cached_axis_sap_tests kyoto_relative_scalar_projection_tests kyoto_pass39_cached_sap_campaign kyoto_pass39_cached_sap_solver kyoto_pass39_projection_solver)
  target_link_libraries(${target} PRIVATE kyoto_contact)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endforeach()
add_test(NAME cached_axis_sap_dynamic_topology_and_reversal COMMAND kyoto_cached_axis_sap_tests)
add_test(NAME relative_scalar_projection_complete_receipt_parity COMMAND kyoto_relative_scalar_projection_tests)
add_test(NAME physical_cached_sap_candidate_smoke COMMAND kyoto_pass39_cached_sap_campaign ${CMAKE_CURRENT_BINARY_DIR}/cached-sap-smoke 1)

# Cross-contact witness reuse and captured Voronoi roundoff regression.
add_executable(kyoto_triangle_barycentric_boundary_tests tests/triangle_barycentric_boundary_tests.cpp)
add_executable(kyoto_pass39_cross_witness_reuse apps/pass39_cross_witness_reuse.cpp)
foreach(target kyoto_triangle_barycentric_boundary_tests kyoto_pass39_cross_witness_reuse)
  target_link_libraries(${target} PRIVATE kyoto_contact)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endforeach()
add_test(NAME triangle_barycentric_boundary_roundoff_regression COMMAND kyoto_triangle_barycentric_boundary_tests)

# Long coupled trajectories, independent exact-volume and upstream CCD audits.
if(KYOTO_WITH_OFFICIAL_IPC AND KYOTO_WITH_EXACT_SELF_VOLUME AND TARGET tight_inclusion::tight_inclusion)
  add_executable(kyoto_pass39_long_physical_audit apps/pass39_long_physical_audit.cpp)
  target_link_libraries(kyoto_pass39_long_physical_audit PRIVATE kyoto_contact tight_inclusion::tight_inclusion)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_pass39_long_physical_audit PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_pass39_long_physical_audit PRIVATE /fp:strict)
  endif()
  add_test(NAME long_coupled_exact_volume_and_upstream_ccd COMMAND kyoto_pass39_long_physical_audit)
endif()

# Bipartite cross-volume candidate sweep: opt-in initial audit backend.
add_executable(kyoto_cross_cell_sweep_tests tests/cross_cell_sweep_tests.cpp)
add_executable(kyoto_pass39_cross_volume_sweep apps/pass39_cross_volume_sweep.cpp)
foreach(target kyoto_cross_cell_sweep_tests kyoto_pass39_cross_volume_sweep)
  target_link_libraries(${target} PRIVATE kyoto_contact)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endforeach()
add_test(NAME bipartite_cross_cell_sweep_exhaustive_parity COMMAND kyoto_cross_cell_sweep_tests)

# Sustained physical leaves with actual IPC LBVH producer and public CCD wrapper.
if(KYOTO_WITH_OFFICIAL_IPC)
  add_executable(kyoto_pass39_long_ipc_wrapper apps/pass39_long_ipc_wrapper.cpp)
  target_link_libraries(kyoto_pass39_long_ipc_wrapper PRIVATE kyoto_contact ipc::toolkit)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_pass39_long_ipc_wrapper PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_pass39_long_ipc_wrapper PRIVATE /fp:strict)
  endif()
  add_test(NAME long_coupled_ipc_wrapper_and_native_candidate_parity COMMAND kyoto_pass39_long_ipc_wrapper)
endif()

# Independent nonlinear contact solver replay (external Python generator).
if(KYOTO_WITH_OFFICIAL_IPC AND KYOTO_WITH_EXACT_SELF_VOLUME)
  add_executable(kyoto_pass40_ipopt_replay apps/pass40_ipopt_replay.cpp)
  target_link_libraries(kyoto_pass40_ipopt_replay PRIVATE kyoto_contact ipc::toolkit)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(kyoto_pass40_ipopt_replay PRIVATE -ffp-contract=off -fno-fast-math)
  elseif(MSVC)
    target_compile_options(kyoto_pass40_ipopt_replay PRIVATE /fp:strict)
  endif()
endif()
add_executable(kyoto_export_gpu_shader tools/export_gpu_shader.cpp)
target_link_libraries(kyoto_export_gpu_shader PRIVATE kyoto_contact)
if(APPLE)
  add_executable(kyoto_pass40_metal_packed_mask apps/pass40_metal_packed_mask.mm)
  set_target_properties(kyoto_pass40_metal_packed_mask PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED ON OBJCXX_EXTENSIONS OFF)
  target_link_libraries(kyoto_pass40_metal_packed_mask PRIVATE kyoto_contact "-framework Foundation" "-framework Metal")
  target_compile_options(kyoto_pass40_metal_packed_mask PRIVATE -fobjc-arc -ffp-contract=off -fno-fast-math)
  add_test(NAME physical_Metal_packed_mask_smoke COMMAND kyoto_pass40_metal_packed_mask ${CMAKE_CURRENT_BINARY_DIR}/pass40-metal-smoke 1)
  set_tests_properties(physical_Metal_packed_mask_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()
