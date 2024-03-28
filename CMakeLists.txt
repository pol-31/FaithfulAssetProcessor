cmake_minimum_required(VERSION 3.26)

project(FaithfulAssetProcessor LANGUAGES CXX C)

cmake_policy(SET CMP0069 NEW)  # LTO support
cmake_policy(SET CMP0091 NEW)  # MSVC runtime support
cmake_policy(SET CMP0075 NEW)  # honor to CMAKE_REQUIRED_LIBRARIES

# to set() variables that are option() in subdirectories (FaithfulAssetProcessor/external/)
cmake_policy(SET CMP0077 NEW)  # honors normal variables

set(CMAKE_WARN_DEPRECATED OFF)

# FaithfulAssetProcessor/installed_libs must be empty - all libs should not be installed and
# variable CMAKE_INSTALL_PREFIX was set to ensure nothing will be installed
# to system dirs (we marked all as EXCLUDE_FROM_ALL but accordingly to CMake
# official documentation it doesn't provide a 100% guarantee)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/installed_libs")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external EXCLUDE_FROM_ALL)

configure_file(${CMAKE_SOURCE_DIR}/config/paths.h.in ${CMAKE_SOURCE_DIR}/config/Paths.h)

add_executable(FaithfulAssetProcessor
        src/main.cpp
        src/AssetLoadingThreadPool.cpp
        src/AssetProcessor.cpp
        src/AssetsAnalyzer.cpp
        src/AudioProcessor.cpp
        src/ModelProcessor.cpp
        src/TextureProcessor.cpp
)

add_dependencies(FaithfulAssetProcessor meshoptimizer)

if(MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(FaithfulAssetProcessor PRIVATE ${CMAKE_CXX_FLAGS}
                /Zi /MTd
        )
    else()
        target_compile_options(FaithfulAssetProcessor PRIVATE ${CMAKE_CXX_FLAGS}
                /Ox /GL /MT
        )
    endif()
elseif((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    target_compile_options(FaithfulAssetProcessor PRIVATE ${CMAKE_CXX_FLAGS}
            -Wall -Wextra
            -Wpedantic -fno-omit-frame-pointer # TODO: -flto
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FAITHFUL_COMPILE_OPTIONS -g ${FAITHFUL_COMPILE_OPTIONS}
                CACHE STRING "Faithful debug compile options (additionally)")
    endif ()
endif()

target_link_libraries(FaithfulAssetProcessor
        PRIVATE stb
        PRIVATE dr_libs
        PRIVATE vorbisenc
        PRIVATE vorbisfile
        PRIVATE vorbis
        PRIVATE ogg
        PRIVATE tinygltf
        PRIVATE astcenc-native-static
)

target_include_directories(FaithfulAssetProcessor
        PRIVATE ${CMAKE_SOURCE_DIR}/external/stb
        PRIVATE ${CMAKE_SOURCE_DIR}/external/dr_libs
        PRIVATE ${CMAKE_SOURCE_DIR}/external/rapidjson/include
        PRIVATE ${CMAKE_SOURCE_DIR}/external/tinygltf
        PRIVATE ${CMAKE_SOURCE_DIR}/external/vorbis/include
        PRIVATE ${CMAKE_SOURCE_DIR}/external/ogg/include
        PRIVATE ${CMAKE_SOURCE_DIR}/external/folly
        PRIVATE ${CMAKE_SOURCE_DIR}/external
)
