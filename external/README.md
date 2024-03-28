
# External libraries

All libraries are build as a static libraries only. For the sake of reduce
memory usage some were added as a git submodule, some were previously uploaded
and simply copied to their folders.

## astc-encoder:
tinyexr.h, stb_image.h, stb_image_write.h
- GitHub: https://github.com/ARM-software/astc-encoder.git
- Branch: main
- License: Apache 2.0
- Include method: git submodule
- Release version: 4.7.0
- Purpose: .astc encoding/decoding (all textures compressed into .astc)
___
## dr_libs
- GitHub: https://github.com/mackron/dr_libs.git
- Branch: master
- License: MIT
- dr_flac: Version 0.12.42
- dr_mp3: Version 0.6.38
- dr_wav - v0.13.15 - 2024-01-23
- Purpose: decompress mp3, flac, wav into PCM
for further compression into ogg+Vorbis
___
## meshoptimizer:
- GitHub: https://github.com/zeux/meshoptimizer.git
- Branch: master
- License: MIT
- Release version: 0.20
- Purpose: optimize gltf meshes (textures not included)
___
## ogg:
- GitHub: https://github.com/xiph/ogg.git
- Branch: master
- License: BSD-3-Clause
- Release version: 1.3.5
- Purpose: ogg + Vorbis encoding/decoding (all audio compressed into .ogg and .wav)
___
## rapidjson
- GitHub: https://github.com/Tencent/rapidjson.git
- Branch: master
- License: BSD
- Release version: 1.1.0
___
## stb
- GitHub: https://github.com/nothings/stb.git
- Branch: master
- License: MIT
- Files: stb_image.h, stb_image_write.h
- Purpose: texture loading for further compression into .astc
___
## tinygltf
- GitHub: https://github.com/syoyo/tinygltf.git
- Branch: release
- License: MIT
- Files: tiny_gltf.h
- Release version: 2.8.21
- Purpose: convert glb/gltf into gltf with external .bin and textures
(textures converted into .astc)
___
## vorbis:
- GitHub: https://github.com/xiph/vorbis.git
- Branch: master
- License: BSD-3-Clause
- Release version: 1.3.7
- Purpose: ogg + Vorbis encoding/decoding (all audio compressed into .ogg and .wav)
___

We've separated "_licenses" folder for convenience, but each library's
folder still contain it own license
