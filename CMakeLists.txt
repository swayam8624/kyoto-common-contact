cmake_minimum_required(VERSION 3.20)
project(kyoto_common_contact LANGUAGES CXX)

# The headless C++ core and its tests should build without GLFW or OpenGL.
# The native showcase targets the user's macOS/CoreText/OpenGL environment.
if(APPLE)
  find_package(glfw3 3.3 REQUIRED)
  find_package(OpenGL REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ccp_core src/core.cpp)
target_include_directories(ccp_core PUBLIC include)
target_compile_options(ccp_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ccp_service apps/ccp_service.cpp)
target_link_libraries(ccp_service PRIVATE ccp_core)

add_library(ccp_live src/live_cloth.cpp)
target_include_directories(ccp_live PUBLIC include)
target_compile_options(ccp_live PRIVATE -Wall -Wextra -Wpedantic)
if(APPLE)
  add_executable(ccp_native MACOSX_BUNDLE native/main.cpp native/font_atlas.cpp)
  target_link_libraries(ccp_native PRIVATE ccp_multicloth glfw OpenGL::GL
    "-framework CoreText" "-framework CoreGraphics" "-framework CoreFoundation")
  target_compile_definitions(ccp_native PRIVATE CCP_OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/output/native")
  set_target_properties(ccp_native PROPERTIES MACOSX_BUNDLE_INFO_PLIST
    "${CMAKE_CURRENT_SOURCE_DIR}/native/Info.plist.in")
endif()
add_executable(ccp_live_benchmark apps/live_benchmark.cpp)
target_link_libraries(ccp_live_benchmark PRIVATE ccp_live)
# Coupled, continually driven multi-cloth experimental broad-phase research.
add_library(ccp_multicloth src/research_multicloth.cpp src/online_partitioner.cpp)
target_link_libraries(ccp_multicloth PUBLIC ccp_live)
target_include_directories(ccp_multicloth PUBLIC include)
target_compile_options(ccp_multicloth PRIVATE -Wall -Wextra -Wpedantic)
# Selective guard can repair isolated cache escapes without full rebuild.
add_executable(ccp_selective_guard_tests tests/selective_guard_tests.cpp)
target_link_libraries(ccp_selective_guard_tests PRIVATE ccp_live)
target_compile_options(ccp_selective_guard_tests PRIVATE -Wall -Wextra -Wpedantic)

# Heterogeneous, deformable mass-spring broad-phase research fixtures.
# Final reusable policy also has an independent direct API oracle regression.
add_executable(ccp_final_broadphase_api_tests tests/final_broadphase_api_tests.cpp)
target_link_libraries(ccp_final_broadphase_api_tests PRIVATE ccp_live)
target_compile_options(ccp_final_broadphase_api_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_material_replay apps/material_replay_benchmark.cpp)
target_link_libraries(ccp_material_replay PRIVATE ccp_multicloth)
target_compile_options(ccp_material_replay PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_material_export apps/material_snapshot_export.cpp)
target_link_libraries(ccp_material_export PRIVATE ccp_multicloth)
target_compile_options(ccp_material_export PRIVATE -Wall -Wextra -Wpedantic)
# Next-stage opt-in held-out geometry/connectivity correctness and real
# coupled-contact solver comparison. Historical material CLI and frozen V4
# executable behavior remain unchanged.
add_executable(ccp_kyoto_heldout_fixture_tests tests/kyoto_heldout_fixture_tests.cpp)
target_link_libraries(ccp_kyoto_heldout_fixture_tests PRIVATE ccp_live)
target_compile_options(ccp_kyoto_heldout_fixture_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_kyoto_coupled_variation_tests tests/kyoto_coupled_variation_tests.cpp)
target_link_libraries(ccp_kyoto_coupled_variation_tests PRIVATE ccp_multicloth)
target_compile_options(ccp_kyoto_coupled_variation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_kyoto_heldout_coupled apps/kyoto_heldout_coupled_audit.cpp)
target_link_libraries(ccp_kyoto_heldout_coupled PRIVATE ccp_multicloth)
target_compile_options(ccp_kyoto_heldout_coupled PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_material_benchmark apps/material_suite_benchmark.cpp)
target_link_libraries(ccp_material_benchmark PRIVATE ccp_multicloth)
target_compile_options(ccp_material_benchmark PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_multicloth_benchmark apps/multicloth_benchmark.cpp)
target_link_libraries(ccp_multicloth_benchmark PRIVATE ccp_multicloth)
# Evidence-only mesh exporter: actual coupled solver positions for every
# supported policy, with per-frame physics diagnostics and SAP state parity.
add_executable(ccp_visual_multicloth_export apps/visual_multicloth_export.cpp)
target_link_libraries(ccp_visual_multicloth_export PRIVATE ccp_multicloth)
target_compile_options(ccp_visual_multicloth_export PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_online_trace apps/online_controller_trace.cpp)
target_link_libraries(ccp_online_trace PRIVATE ccp_multicloth)
add_executable(ccp_cloth_stability_probe apps/cloth_stability_probe.cpp)
target_link_libraries(ccp_cloth_stability_probe PRIVATE ccp_multicloth)
add_executable(ccp_patch_sweep apps/patch_granularity_benchmark.cpp)
target_link_libraries(ccp_patch_sweep PRIVATE ccp_multicloth)



# Independent, published-family 3D baseline: actual Bullet DBVT library.
# Without Bullet, the project still builds its own core and tests, but does
# NOT claim to have compared against a published external implementation.
option(CCP_REQUIRE_BULLET "Fail configuration if the Bullet reference is unavailable" OFF)
find_package(Bullet QUIET)
if(Bullet_FOUND)
  target_link_libraries(ccp_material_replay PRIVATE ${BULLET_LIBRARIES})
  target_include_directories(ccp_material_replay PRIVATE ${BULLET_INCLUDE_DIRS})
  target_compile_definitions(ccp_material_replay PRIVATE CCP_HAVE_BULLET=1)
  add_executable(ccp_broadphase_leaderboard apps/broadphase_leaderboard.cpp)
  target_link_libraries(ccp_broadphase_leaderboard PRIVATE ccp_live ccp_multicloth ${BULLET_LIBRARIES})
  target_include_directories(ccp_broadphase_leaderboard PRIVATE ${BULLET_INCLUDE_DIRS})
  target_compile_options(ccp_broadphase_leaderboard PRIVATE -Wall -Wextra -Wpedantic)
  message(STATUS "External 3D broad-phase benchmark: Bullet DBVT enabled")
elseif(CCP_REQUIRE_BULLET)
  message(FATAL_ERROR "Bullet 3D reference requested but not installed; install libbullet-dev or brew install bullet")
else()
  message(STATUS "External 3D broad-phase benchmark unavailable: install Bullet to enable it")
endif()

add_executable(common_bounds_demo apps/common_bounds_demo.cpp)
target_compile_options(common_bounds_demo PRIVATE -Wall -Wextra -Wpedantic)

add_executable(mesh_bounds_query apps/mesh_bounds_query.cpp)
target_compile_options(mesh_bounds_query PRIVATE -Wall -Wextra -Wpedantic)


# Standalone deterministic CPU scaling reference. This is intentionally
# distinct from the coupled deformable simulation and the adaptive controller.
find_package(Threads REQUIRED)
# Selective local cache repair can distribute large dirty subsets across
# independent CPU workers; use portable thread linkage on Linux/macOS.
target_link_libraries(ccp_material_benchmark PRIVATE Threads::Threads)
target_link_libraries(ccp_material_replay PRIVATE Threads::Threads)
target_link_libraries(ccp_selective_guard_tests PRIVATE Threads::Threads)
add_executable(ccp_scaling_regional apps/scaling_regional.cpp)
target_link_libraries(ccp_scaling_regional PRIVATE ccp_multicloth)
target_compile_options(ccp_scaling_regional PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_scaling_cpu apps/scaling_cpu.cpp)
target_link_libraries(ccp_scaling_cpu PRIVATE Threads::Threads)
target_compile_options(ccp_scaling_cpu PRIVATE -Wall -Wextra -Wpedantic)

# Native Apple GPU reference uses actual Metal compute, not simulated GPU data.
# Its exhaustive kernel is a correctness/performance control, not an optimized
# counterpart to the adaptive CPU approach.
if(APPLE)
  enable_language(OBJCXX)
  add_executable(ccp_scaling_metal apps/scaling_metal.mm)
  target_link_libraries(ccp_scaling_metal PRIVATE "-framework Foundation" "-framework Metal")
  target_compile_options(ccp_scaling_metal PRIVATE -Wall -Wextra -Wpedantic)
  # Phase 11: REAL Apple GPU budgeted adversarial 3D hashing vs all-pairs
  # search, both count-only to avoid TB-scale pair materialization.
  add_executable(ccp_cbp_pressure_metal apps/cbp_pressure_metal.mm)
  target_include_directories(ccp_cbp_pressure_metal PRIVATE include)
  target_link_libraries(ccp_cbp_pressure_metal PRIVATE
    "-framework Foundation" "-framework Metal")
  target_compile_options(ccp_cbp_pressure_metal PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Experimental CBP: Phase-0 exhaustive control and Phase-1 shared spatial BVH.
# These header-only targets do not need the native renderer, Bullet or ML.
add_executable(ccp_cbp_router_tests tests/cbp_region_router_tests.cpp)
target_include_directories(ccp_cbp_router_tests PRIVATE include)
target_compile_options(ccp_cbp_router_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ccp_cbp_shared_bvh_tests tests/cbp_shared_bvh_tests.cpp)
target_include_directories(ccp_cbp_shared_bvh_tests PRIVATE include)
target_compile_options(ccp_cbp_shared_bvh_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_certified_cache_tests tests/cbp_certified_cache_tests.cpp)
target_include_directories(ccp_cbp_certified_cache_tests PRIVATE include)
target_compile_options(ccp_cbp_certified_cache_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_cross_bench apps/cbp_cross_microbench.cpp)
target_include_directories(ccp_cbp_cross_bench PRIVATE include)
target_compile_options(ccp_cbp_cross_bench PRIVATE -Wall -Wextra -Wpedantic)
# Phase-3 causal single-backend manager: independent oracle and short trace.
add_executable(ccp_cbp_cost_manager_tests tests/cbp_cost_manager_tests.cpp)
target_include_directories(ccp_cbp_cost_manager_tests PRIVATE include)
target_compile_options(ccp_cbp_cost_manager_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_manager_bench apps/cbp_manager_microbench.cpp)
target_include_directories(ccp_cbp_manager_bench PRIVATE include)
target_compile_options(ccp_cbp_manager_bench PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 7: hundreds/thousands of independently stepped material assemblies
# serialized once and replayed identically through CPU and Apple Metal.
add_executable(ccp_cbp_mass_export apps/cbp_mass_export.cpp)
target_link_libraries(ccp_cbp_mass_export PRIVATE ccp_multicloth)
target_compile_options(ccp_cbp_mass_export PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase-8 mesh-truth re-simulator proves vertex/bond visual data match
# the original immutable broad-phase Float32 snapshots.
add_executable(ccp_cbp_mesh_truth_export apps/cbp_mesh_truth_export.cpp)
target_link_libraries(ccp_cbp_mesh_truth_export PRIVATE ccp_multicloth)
target_compile_options(ccp_cbp_mesh_truth_export PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Independent bounded large-scale AABB family; NOT a coupled million-mesh sim.
add_executable(ccp_cbp_million_scale apps/cbp_million_scale.cpp)
target_compile_options(ccp_cbp_million_scale PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 11: ACTUAL adversarial global 3D searches. No proven-separated islands.
# Count-only storage prevents all-overlap N^2 output allocation catastrophes.
# Phase 13: independent geometry-cost portfolio for the same global stress geometry.
# Phase 14: retain actual spatial cell ownership across changing frame geometry
# rather than rebuilding an unordered_map for every full-frame query.
# Phase 15 preserves Phase 14's pointer-heavy version as a negative control.
# The flat version uses a contiguous vector of spatial cells and separate
# map-only update lookup, so pair enumeration is cache-local.
add_executable(ccp_cbp_pressure_incremental_flat_cpu apps/cbp_pressure_incremental_flat_cpu.cpp)
target_include_directories(ccp_cbp_pressure_incremental_flat_cpu PRIVATE include)
find_package(Threads REQUIRED)
target_compile_options(ccp_cbp_pressure_incremental_flat_cpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(ccp_cbp_pressure_incremental_flat_cpu PRIVATE Threads::Threads)
# Phase 17: actual full pair-output streaming from the independently retained
# persistent flat-cell index, with bounded disk sink and exact digest checks.
add_executable(ccp_cbp_pressure_incremental_flat_emit apps/cbp_pressure_incremental_flat_emit.cpp)
target_include_directories(ccp_cbp_pressure_incremental_flat_emit PRIVATE include)
target_compile_options(ccp_cbp_pressure_incremental_flat_emit PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(ccp_cbp_pressure_incremental_flat_emit PRIVATE Threads::Threads)
add_executable(ccp_cbp_pressure_incremental_flat_tests tests/cbp_pressure_incremental_flat_tests.cpp)
target_include_directories(ccp_cbp_pressure_incremental_flat_tests PRIVATE include)
target_compile_options(ccp_cbp_pressure_incremental_flat_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(ccp_cbp_pressure_incremental_flat_tests PRIVATE Threads::Threads)
add_executable(ccp_cbp_pressure_incremental_cpu apps/cbp_pressure_incremental_cpu.cpp)
target_include_directories(ccp_cbp_pressure_incremental_cpu PRIVATE include)
target_compile_options(ccp_cbp_pressure_incremental_cpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_pressure_incremental_tests tests/cbp_pressure_incremental_tests.cpp)
target_include_directories(ccp_cbp_pressure_incremental_tests PRIVATE include)
target_compile_options(ccp_cbp_pressure_incremental_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_pressure_hybrid_cpu apps/cbp_pressure_hybrid_cpu.cpp)
target_include_directories(ccp_cbp_pressure_hybrid_cpu PRIVATE include)
target_compile_options(ccp_cbp_pressure_hybrid_cpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_pressure_cpu apps/cbp_pressure_cpu.cpp)
target_include_directories(ccp_cbp_pressure_cpu PRIVATE include)
target_compile_options(ccp_cbp_pressure_cpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 11: genuinely write complete unique 8-byte candidate pairs under
# explicit per-frame memory/disk caps, separately from count-only timing.
add_executable(ccp_cbp_pressure_emit apps/cbp_pressure_emit.cpp)
target_include_directories(ccp_cbp_pressure_emit PRIVATE include)
target_compile_options(ccp_cbp_pressure_emit PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Vulkan/MoltenVK optional cross-API GPU reference. The SPIR-V compiler is
# REQUIRED for this target; no fake GPU/backend selection or silent fallback.
# On macOS: LunarG Vulkan SDK + MoltenVK and glslangValidator.
# On Linux: Vulkan SDK; Lavapipe is SOFTWARE and is labelled as such.
find_package(Vulkan QUIET)
find_program(CCP_GLSLANG_VALIDATOR glslangValidator)
if(Vulkan_FOUND AND CCP_GLSLANG_VALIDATOR)
  set(CCP_PRESSURE_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/cbp_pressure_count.comp.spv")
  add_custom_command(OUTPUT "${CCP_PRESSURE_SPIRV}"
    COMMAND "${CCP_GLSLANG_VALIDATOR}" -V
      "${CMAKE_CURRENT_SOURCE_DIR}/shaders/cbp_pressure_count.comp"
      -o "${CCP_PRESSURE_SPIRV}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/cbp_pressure_count.comp"
    COMMENT "Compile true Vulkan adversarial AABB SPIR-V kernel"
    VERBATIM)
  add_custom_target(ccp_cbp_pressure_spirv DEPENDS "${CCP_PRESSURE_SPIRV}")
  add_executable(ccp_cbp_pressure_vulkan apps/cbp_pressure_vulkan.cpp)
  add_dependencies(ccp_cbp_pressure_vulkan ccp_cbp_pressure_spirv)
  target_link_libraries(ccp_cbp_pressure_vulkan PRIVATE Vulkan::Vulkan)
  target_include_directories(ccp_cbp_pressure_vulkan PRIVATE include)
  target_compile_definitions(ccp_cbp_pressure_vulkan PRIVATE
    CCP_PRESSURE_SPIRV="${CCP_PRESSURE_SPIRV}")
  # Vk* structs use Vulkan-mandated sType-first aggregate initialization;
  # unspecified trailing fields are intentionally zero-initialized.
  target_compile_options(ccp_cbp_pressure_vulkan PRIVATE
    -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Werror)
  message(STATUS "CBP Phase 11 native Vulkan pressure: compiled target available")
else()
  message(STATUS "CBP Phase 11 Vulkan benchmark unavailable: install Vulkan SDK + glslangValidator (no GPU result inferred)")
endif()
add_executable(ccp_cbp_mass_replay apps/cbp_mass_replay.cpp)
target_link_libraries(ccp_cbp_mass_replay PRIVATE ccp_live)
target_include_directories(ccp_cbp_mass_replay PRIVATE include)
target_compile_options(ccp_cbp_mass_replay PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Exact sampled cross-instance AABB intersection locations for research viewers.
add_executable(ccp_cbp_contact_hotspots apps/cbp_contact_hotspots.cpp)
target_link_libraries(ccp_cbp_contact_hotspots PRIVATE ccp_live)
target_include_directories(ccp_cbp_contact_hotspots PRIVATE include)
target_compile_options(ccp_cbp_contact_hotspots PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 6 separate opt-in X-swept region-pair join.
add_executable(ccp_cbp_region_sweep_tests tests/cbp_region_sweep_tests.cpp)
target_include_directories(ccp_cbp_region_sweep_tests PRIVATE include)
target_compile_options(ccp_cbp_region_sweep_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 4: exact frozen-V4 fallback with bounded on-policy regional trials.
add_executable(ccp_cbp_portfolio_tests tests/cbp_portfolio_tests.cpp)
target_link_libraries(ccp_cbp_portfolio_tests PRIVATE ccp_live)
target_include_directories(ccp_cbp_portfolio_tests PRIVATE include)
target_compile_options(ccp_cbp_portfolio_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Phase 10 independent four-arm causal controller. Preserve old portfolio unchanged.
add_executable(ccp_cbp_portfolio_v2_tests tests/cbp_portfolio_v2_tests.cpp)
target_link_libraries(ccp_cbp_portfolio_v2_tests PRIVATE ccp_live)
target_include_directories(ccp_cbp_portfolio_v2_tests PRIVATE include)
target_compile_options(ccp_cbp_portfolio_v2_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_cbp_portfolio_v2_replay apps/cbp_portfolio_v2_replay.cpp)
target_link_libraries(ccp_cbp_portfolio_v2_replay PRIVATE ccp_live)
target_include_directories(ccp_cbp_portfolio_v2_replay PRIVATE include)
target_compile_options(ccp_cbp_portfolio_v2_replay PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Actual mesh topology IDs from a coupled multi-cloth solver; same-input
# broad-phase comparisons, separate from solver advance and rendering.
add_executable(ccp_cbp_topology_replay apps/cbp_topology_replay.cpp)
target_link_libraries(ccp_cbp_topology_replay PRIVATE ccp_multicloth)
target_include_directories(ccp_cbp_topology_replay PRIVATE include)
target_compile_options(ccp_cbp_topology_replay PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Identical immutable KYOTOM01 material replays against frozen V4.
add_executable(ccp_cbp_kmcb_replay apps/cbp_kmcb_replay.cpp)
target_link_libraries(ccp_cbp_kmcb_replay PRIVATE ccp_live)
target_include_directories(ccp_cbp_kmcb_replay PRIVATE include)
target_compile_options(ccp_cbp_kmcb_replay PRIVATE -Wall -Wextra -Wpedantic -Werror)

# RBC / Reach-Budget Calculus Stage 0: independent O(N^2)
# reference-certificate correctness test. This is intentionally NOT
# a new production broad phase, physics solver or benchmarked speed claim.
# Frozen final_broadphase.hpp remains untouched.
add_executable(ccp_rbc_reach_budget_tests tests/rbc_reach_budget_tests.cpp)
target_include_directories(ccp_rbc_reach_budget_tests PRIVATE include)
target_compile_options(ccp_rbc_reach_budget_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_rbc_margin_model_tests tests/rbc_margin_model_tests.cpp)
target_include_directories(ccp_rbc_margin_model_tests PRIVATE include)
target_compile_options(ccp_rbc_margin_model_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_rbc_swept_reach_tests tests/rbc_swept_reach_tests.cpp)
target_include_directories(ccp_rbc_swept_reach_tests PRIVATE include)
target_compile_options(ccp_rbc_swept_reach_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_rbc_spatial_frontier_tests tests/rbc_spatial_frontier_tests.cpp)
target_include_directories(ccp_rbc_spatial_frontier_tests PRIVATE include)
target_compile_options(ccp_rbc_spatial_frontier_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_executable(ccp_rbc_material_tournament apps/rbc_material_tournament.cpp)
target_link_libraries(ccp_rbc_material_tournament PRIVATE ccp_live)
target_include_directories(ccp_rbc_material_tournament PRIVATE include)
target_compile_options(ccp_rbc_material_tournament PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
# Genuine physically stepped solver. Do NOT conflate AABB-only query
# improvement with this separately measured full XPBD-style step.
add_executable(ccp_rbc_coupled_tournament apps/rbc_coupled_tournament.cpp)
target_link_libraries(ccp_rbc_coupled_tournament PRIVATE ccp_multicloth)
target_include_directories(ccp_rbc_coupled_tournament PRIVATE include)
target_compile_options(ccp_rbc_coupled_tournament PRIVATE
  -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME rbc_exact_reach_certificate_and_invalidation_oracle
    COMMAND ccp_rbc_reach_budget_tests)
  add_test(NAME rbc_analytic_margin_proxy_formula
    COMMAND ccp_rbc_margin_model_tests)
  add_test(NAME rbc_linear_swept_reach_conservative_candidate_contract
    COMMAND ccp_rbc_swept_reach_tests)
  add_test(NAME rbc_indexed_spatial_frontier_exact_pair_and_local_repair
    COMMAND ccp_rbc_spatial_frontier_tests)
  # Full real coupled experiment and immutable KYOTOM01 replay run in
  # GitHub Actions with NEW no-overwrite evidence directories; not in
  # normal CTest to avoid polluting existing source or repeat failures.
  add_test(NAME cbp_exact_region_oracle COMMAND ccp_cbp_router_tests)
  add_test(NAME cbp_shared_bvh_oracle COMMAND ccp_cbp_shared_bvh_tests)
  add_test(NAME cbp_certified_graph_oracle COMMAND ccp_cbp_certified_cache_tests)
  add_test(NAME cbp_causal_manager_oracle COMMAND ccp_cbp_cost_manager_tests)
  add_test(NAME cbp_portfolio_fallback_oracle COMMAND ccp_cbp_portfolio_tests)
  add_test(NAME cbp_four_arm_portfolio_oracle COMMAND ccp_cbp_portfolio_v2_tests)

  add_test(NAME cbp_region_sweep_oracle COMMAND ccp_cbp_region_sweep_tests)
  add_test(NAME final_broadphase_public_api_exact_pair_oracle COMMAND ccp_final_broadphase_api_tests)
  add_test(NAME kyoto_heldout_independent_geometry_topology_oracle
    COMMAND ccp_kyoto_heldout_fixture_tests)
  add_test(NAME kyoto_coupled_contact_independent_mesh_topology_oracle
    COMMAND ccp_kyoto_coupled_variation_tests)
  # Coupled multi-policy fold integration is exercised by the additive
  # held-out CI runner, which allocates a NEW output directory. Do not run
  # the native no-overwrite evidence exporter as a CTest against a fixed
  # directory: second CTest invocation would fail despite identical code.
  add_test(NAME selective_guard_exact_pair_oracle COMMAND ccp_selective_guard_tests)
  add_test(NAME deformable_material_suite_oracle
    COMMAND ${CMAKE_COMMAND} -E env CCP_CTEST_SMOKE=1
      $<TARGET_FILE:ccp_material_benchmark> ${CMAKE_CURRENT_BINARY_DIR}/material-ci-smoke.csv all 48 4 1234 1 0)
  add_test(NAME scaling_cpu_parallel_oracle COMMAND ccp_scaling_cpu --self-test)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cbp_flat_persistent_parallel_independent_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_pressure_parallel.py
        $<TARGET_FILE:ccp_cbp_pressure_incremental_flat_cpu>
        $<TARGET_FILE:ccp_cbp_pressure_cpu>)
    add_test(NAME cbp_flat_persistent_cell_direct_oracle
      COMMAND ccp_cbp_pressure_incremental_flat_tests)
    add_test(NAME cbp_flat_persistent_cell_independent_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_pressure_incremental_flat.py
        $<TARGET_FILE:ccp_cbp_pressure_cpu>
        $<TARGET_FILE:ccp_cbp_pressure_incremental_flat_cpu>)
    add_test(NAME cbp_persistent_cell_direct_oracle
      COMMAND ccp_cbp_pressure_incremental_tests)
    add_test(NAME cbp_persistent_cell_independent_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_pressure_incremental.py
        $<TARGET_FILE:ccp_cbp_pressure_cpu>
        $<TARGET_FILE:ccp_cbp_pressure_incremental_cpu>)
    add_test(NAME cbp_geometric_router_exact_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_pressure_hybrid.py
        $<TARGET_FILE:ccp_cbp_pressure_cpu>
        $<TARGET_FILE:ccp_cbp_pressure_hybrid_cpu>)
    add_test(NAME cbp_adversarial_pressure_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_pressure_cpu.py
        $<TARGET_FILE:ccp_cbp_pressure_cpu>)
    add_test(NAME learned_cpp_model_and_exact_pair_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_learned_cxx.py
        $<TARGET_FILE:ccp_material_export>
        $<TARGET_FILE:ccp_material_replay>)
    add_test(NAME evidence_csv_contracts
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_evidence_summarizers.py)
    add_test(NAME cbp_many_material_worlds_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_mass_stress.py
        $<TARGET_FILE:ccp_cbp_mass_export> $<TARGET_FILE:ccp_cbp_mass_replay>)
    add_test(NAME cbp_spatial_hotspots_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_contact_hotspots.py
        $<TARGET_FILE:ccp_cbp_mass_export> $<TARGET_FILE:ccp_cbp_mass_replay>
        $<TARGET_FILE:ccp_cbp_contact_hotspots>)
    add_test(NAME cbp_real_mesh_truth_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_mesh_truth.py
        $<TARGET_FILE:ccp_cbp_mass_export> $<TARGET_FILE:ccp_cbp_mesh_truth_export>)
    add_test(NAME cbp_real_mesh_observatory_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_mesh_observatory.py
        $<TARGET_FILE:ccp_cbp_mass_export> $<TARGET_FILE:ccp_cbp_mass_replay>
        $<TARGET_FILE:ccp_cbp_mesh_truth_export>)
    add_test(NAME cbp_mesh_topology_replay_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_topology_replay.py
        $<TARGET_FILE:ccp_cbp_topology_replay>)
    add_test(NAME cbp_kmcb_snapshot_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cbp_kmcb_smoke.py
        $<TARGET_FILE:ccp_cbp_kmcb_replay>)
  endif()
  add_executable(ccp_multicloth_tests tests/research_multicloth_tests.cpp)
  target_link_libraries(ccp_multicloth_tests PRIVATE ccp_multicloth)
  add_test(NAME moving_multicloth_regional_coverage COMMAND ccp_multicloth_tests)
  if(TARGET ccp_broadphase_leaderboard)
    add_test(NAME published_3d_broadphase_coverage
      COMMAND ccp_broadphase_leaderboard --self-test)
  endif()
  add_executable(ccp_live_tests tests/live_cloth_tests.cpp)
  target_link_libraries(ccp_live_tests PRIVATE ccp_live)
  add_test(NAME live_cloth_contracts COMMAND ccp_live_tests)
  add_executable(ccp_tests tests/core_tests.cpp)
  target_link_libraries(ccp_tests PRIVATE ccp_core)
  add_test(NAME ccp_core_contracts COMMAND ccp_tests)
  add_test(NAME common_bounds_contract
    COMMAND common_bounds_demo ${CMAKE_CURRENT_BINARY_DIR}/verified_common_bounds.html)
  add_test(NAME mesh_bounds_query_contract COMMAND mesh_bounds_query --self-test)
endif()
