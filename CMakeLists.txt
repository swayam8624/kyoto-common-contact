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

include(CTest)
if(BUILD_TESTING)
  add_test(NAME cbp_exact_region_oracle COMMAND ccp_cbp_router_tests)
  add_test(NAME cbp_shared_bvh_oracle COMMAND ccp_cbp_shared_bvh_tests)
  add_test(NAME cbp_certified_graph_oracle COMMAND ccp_cbp_certified_cache_tests)
  add_test(NAME final_broadphase_public_api_exact_pair_oracle COMMAND ccp_final_broadphase_api_tests)
  add_test(NAME selective_guard_exact_pair_oracle COMMAND ccp_selective_guard_tests)
  add_test(NAME deformable_material_suite_oracle
    COMMAND ${CMAKE_COMMAND} -E env CCP_CTEST_SMOKE=1
      $<TARGET_FILE:ccp_material_benchmark> ${CMAKE_CURRENT_BINARY_DIR}/material-ci-smoke.csv all 48 4 1234 1 0)
  add_test(NAME scaling_cpu_parallel_oracle COMMAND ccp_scaling_cpu --self-test)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME learned_cpp_model_and_exact_pair_oracle
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_learned_cxx.py
        $<TARGET_FILE:ccp_material_export>
        $<TARGET_FILE:ccp_material_replay>)
    add_test(NAME evidence_csv_contracts
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_evidence_summarizers.py)
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
