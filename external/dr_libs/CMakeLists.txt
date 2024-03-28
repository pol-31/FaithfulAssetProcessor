
configure_file(dr_wav.h dr_wav.cpp COPYONLY)
configure_file(dr_flac.h dr_flac.cpp COPYONLY)
configure_file(dr_mp3.h dr_mp3.cpp COPYONLY)
add_library(dr_libs STATIC dr_wav.cpp dr_flac.cpp dr_mp3.cpp)
target_compile_definitions(dr_libs PRIVATE
        DR_WAV_IMPLEMENTATION
        DR_FLAC_IMPLEMENTATION
        DR_MP3_IMPLEMENTATION)
