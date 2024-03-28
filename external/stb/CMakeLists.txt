
configure_file(stb_image.h stb_image.cpp COPYONLY)
configure_file(stb_image_write.h stb_image_write.cpp COPYONLY)
add_library(stb STATIC stb_image.cpp stb_image_write.cpp)
target_compile_definitions(stb PRIVATE
        STB_IMAGE_IMPLEMENTATION
        STB_IMAGE_WRITE_IMPLEMENTATION
)
