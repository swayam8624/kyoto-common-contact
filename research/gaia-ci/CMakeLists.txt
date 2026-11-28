cmake_minimum_required(VERSION 3.16)
project(GaiaIndependentCPU LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Eigen3 3.4.0 EXACT CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(embree 3 CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
set(M "${GAIA_SOURCE}/Simulator/Modules")
set(D "${GAIA_SOURCE}/Simulator/3rdParty")
find_package(MeshFrame2 CONFIG REQUIRED PATHS "${D}/MeshFrame2/MeshFrame/cmake" NO_DEFAULT_PATH)
add_executable(gaia_probe
  baseline.cpp
  "${M}/CollisionDetector/ClothContactDetector.cpp"
  "${M}/CollisionDetector/TriMeshCollisionGeometry.cpp"
  "${M}/CollisionDetector/CollisionGeometry.cpp"
  "${M}/TriMesh/TriMesh.cpp"
  "${M}/common/math/constants.cpp"
  "${M}/common/simd/sse.cpp"
  ${MESHFRAME_SOURCE_CPP_UTILITY})
target_include_directories(gaia_probe PRIVATE "${M}" "${D}/CuMatrix" ${MESHFRAME_INCLUDE_DIR})
target_compile_definitions(gaia_probe PRIVATE GAIA_NO_GUI KYOTO_CUMATRIX_HOST_ONLY KYOTO_GAIA_CLOTH_ONLY)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(gaia_probe PRIVATE -ferror-limit=0)
endif()
target_link_libraries(gaia_probe PRIVATE Eigen3::Eigen TBB::tbb embree OpenMP::OpenMP_CXX ${CMAKE_DL_LIBS})
