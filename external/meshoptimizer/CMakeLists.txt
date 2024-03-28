cmake_minimum_required(VERSION 3.26)

# we need external project here, because we need "BUILD_BYPRODUCTS"
# we don't need meshoptimizer but gltfpack cli only, so we don't include it
# to any target and CMake consider it as a useless and don't compile it

include(ExternalProject)

set(MESHOPTIMIZER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/meshoptimizer/meshoptimizer)
set(MESHOPTIMIZER_BINARY_DIR ${CMAKE_BINARY_DIR}/external/meshoptimizer)

ExternalProject_Add(
        meshoptimizer_external
        BINARY_DIR ${MESHOPTIMIZER_BINARY_DIR}
        SOURCE_DIR ${MESHOPTIMIZER_SOURCE_DIR}
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DMESHOPT_BUILD_GLTFPACK=ON
        ${MESHOPTIMIZER_SOURCE_DIR}
        BUILD_COMMAND ${CMAKE_COMMAND} --build .
        INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
        BUILD_BYPRODUCTS ${MESHOPTIMIZER_BINARY_DIR}/gltfpack
)

set(FAITHFUL_ASSET_PROCESSOR_GLTFPACK_PATH "${MESHOPTIMIZER_BINARY_DIR}/gltfpack" CACHE STRING "" FORCE)

add_library(meshoptimizer INTERFACE)
add_dependencies(meshoptimizer meshoptimizer_external)
