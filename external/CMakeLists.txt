cmake_minimum_required(VERSION 3.26)

set(FAITHFUL_BUILD_ASTC_ENCODER ON CACHE INTERNAL "")
message(STATUS "---> astc-encoder building")
set(ASTCENC_BLOCK_MAX_TEXELS 64)
add_subdirectory(astc-encoder)

message(STATUS "---> dr_libs building")
add_subdirectory(dr_libs EXCLUDE_FROM_ALL)

message(STATUS "---> libogg building")
add_subdirectory(ogg EXCLUDE_FROM_ALL)

#    rapidjson (header-only library, so included directly)

message(STATUS "---> stb_image, stb_image_write building")
add_subdirectory(stb EXCLUDE_FROM_ALL)

message(STATUS "---> tinygltf building")
add_subdirectory(tinygltf EXCLUDE_FROM_ALL)

message(STATUS "---> vorbis building")
add_subdirectory(vorbis EXCLUDE_FROM_ALL)

message(STATUS "meshoptimizer building")
add_subdirectory(meshoptimizer EXCLUDE_FROM_ALL)

#    folly (need only headers, so included directly)
