
configure_file(tiny_gltf.h tinygltf.cpp COPYONLY)
add_library(tinygltf STATIC tinygltf.cpp)
target_compile_definitions(tinygltf PRIVATE TINYGLTF_IMPLEMENTATION)
target_include_directories(tinygltf
        PRIVATE ../rapidjson
        PRIVATE ../stb
)
