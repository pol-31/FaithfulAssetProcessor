cmake_minimum_required(VERSION 3.5)

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW) # Enables override of options from parent CMakeLists.txt
endif()

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW) # Enables use of MSVC_RUNTIME_LIBRARY
endif()

if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW) # Enables clean /W4 override for MSVC
endif()

project(meshoptimizer VERSION 0.20 LANGUAGES CXX)

option(MESHOPT_BUILD_GLTFPACK "Build gltfpack" OFF)
option(MESHOPT_WERROR "Treat warnings as errors" OFF)
set(MESHOPT_BASISU_PATH "" CACHE STRING "")

set(SOURCES
    src/meshoptimizer.h
    src/allocator.cpp
    src/clusterizer.cpp
    src/indexcodec.cpp
    src/indexgenerator.cpp
    src/overdrawanalyzer.cpp
    src/overdrawoptimizer.cpp
    src/quantization.cpp
    src/simplifier.cpp
    src/spatialorder.cpp
    src/stripifier.cpp
    src/vcacheanalyzer.cpp
    src/vcacheoptimizer.cpp
    src/vertexcodec.cpp
    src/vertexfilter.cpp
    src/vfetchanalyzer.cpp
    src/vfetchoptimizer.cpp
)

set(GLTF_SOURCES
    gltf/animation.cpp
    gltf/basisenc.cpp
    gltf/basislib.cpp
    gltf/fileio.cpp
    gltf/gltfpack.cpp
    gltf/image.cpp
    gltf/json.cpp
    gltf/material.cpp
    gltf/mesh.cpp
    gltf/node.cpp
    gltf/parseobj.cpp
    gltf/parselib.cpp
    gltf/parsegltf.cpp
    gltf/stream.cpp
    gltf/write.cpp
)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wshadow -Wno-missing-field-initializers)
endif()

if(MESHOPT_WERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

add_library(meshoptimizer STATIC ${SOURCES})

target_include_directories(meshoptimizer INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

set(TARGETS meshoptimizer)

if(MESHOPT_BUILD_GLTFPACK)
    add_executable(gltfpack ${GLTF_SOURCES})
    set_target_properties(gltfpack PROPERTIES CXX_STANDARD 11)
    target_link_libraries(gltfpack meshoptimizer)
    list(APPEND TARGETS gltfpack)

    if(NOT MESHOPT_BASISU_PATH STREQUAL "")
        get_filename_component(BASISU_PATH ${MESHOPT_BASISU_PATH} ABSOLUTE)

        target_compile_definitions(gltfpack PRIVATE WITH_BASISU)
        set_source_files_properties(gltf/basisenc.cpp gltf/basislib.cpp PROPERTIES INCLUDE_DIRECTORIES ${BASISU_PATH})

        if(NOT MSVC AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set_source_files_properties(gltf/basislib.cpp PROPERTIES COMPILE_OPTIONS -msse4.1)
        endif()

        if(UNIX)
            target_link_libraries(gltfpack pthread)
        endif()
    endif()
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/meshoptimizerConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/meshoptimizer NO_SET_AND_CHECK_MACRO)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/meshoptimizerConfigVersion.cmake COMPATIBILITY ExactVersion)
