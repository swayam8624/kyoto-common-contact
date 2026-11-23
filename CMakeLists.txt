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

  # Pass 5: official upstream CCD on genuine solver-integrated
  # tetrahedral trajectories, reporting unresolved self-contact.
  add_executable(kyoto_official_ipc_solid_audit tests/ipc_solid_trajectory_audit.cpp)
  target_link_libraries(kyoto_official_ipc_solid_audit PRIVATE kyoto_contact ipc::toolkit)
  add_test(NAME real_hyperelastic_solver_ipc_geometry_audit
           COMMAND kyoto_official_ipc_solid_audit ${CMAKE_CURRENT_BINARY_DIR}/ipc-solid-audit)


endif()
