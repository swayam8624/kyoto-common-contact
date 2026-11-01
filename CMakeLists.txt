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
  target_link_libraries(ccp_native PRIVATE ccp_live ccp_multicloth glfw OpenGL::GL
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
add_executable(ccp_multicloth_benchmark apps/multicloth_benchmark.cpp)
target_link_libraries(ccp_multicloth_benchmark PRIVATE ccp_multicloth)
add_executable(ccp_online_trace apps/online_controller_trace.cpp)
target_link_libraries(ccp_online_trace PRIVATE ccp_multicloth)
add_executable(ccp_patch_sweep apps/patch_granularity_benchmark.cpp)
target_link_libraries(ccp_patch_sweep PRIVATE ccp_multicloth)



# Independent, published-family 3D baseline: actual Bullet DBVT library.
# Without Bullet, the project still builds its own core and tests, but does
# NOT claim to have compared against a published external implementation.
option(CCP_REQUIRE_BULLET "Fail configuration if the Bullet reference is unavailable" OFF)
find_package(Bullet QUIET)
if(Bullet_FOUND)
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

include(CTest)
if(BUILD_TESTING)
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
