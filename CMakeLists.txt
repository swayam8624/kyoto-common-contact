cmake_minimum_required(VERSION 3.20)
project(kyoto_common_contact LANGUAGES CXX)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

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
add_executable(ccp_native MACOSX_BUNDLE native/main.cpp native/font_atlas.cpp)
target_link_libraries(ccp_native PRIVATE ccp_live glfw OpenGL::GL "-framework CoreText" "-framework CoreGraphics" "-framework CoreFoundation")
add_executable(ccp_live_benchmark apps/live_benchmark.cpp)
target_link_libraries(ccp_live_benchmark PRIVATE ccp_live)
target_compile_definitions(ccp_native PRIVATE CCP_OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/output/native")
set_target_properties(ccp_native PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/native/Info.plist.in")

add_executable(common_bounds_demo apps/common_bounds_demo.cpp)
target_compile_options(common_bounds_demo PRIVATE -Wall -Wextra -Wpedantic)

add_executable(mesh_bounds_query apps/mesh_bounds_query.cpp)
target_compile_options(mesh_bounds_query PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
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
