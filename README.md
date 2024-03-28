# FaithfulAssetProcessor

---
### Purpose:
Designed for [Faithful](https://github.com/pol-31/Faithful) game that
works only with certain formats of assets:

Audio: Ogg/Vorbis for music and .wav for sounds (we distinguish them by size -
at least wanted to do this)

Models: .gltf (only 1 material with 5 textures: albedo, metal_rough, 
emission, ao, normal) + external .bin and .astc textures. Because
.astc is not supported by GLTF 2.0 spec, we handle it on our own.

Textures:
* encode to .astc (both hdr and ldr; to distinguish them we add prefix
hdr_ before file name), decode to .png or .hdr
* supported formats: bmp, hdr, HDR, jpeg, jpg, pgm, png, ppm, psd, tga (just
copied from stb_image.h)
* astc params: 4x4 compression ASTCENC_PRE_MEDIUM, uint8 for ldr and float32 for hdr (
* From [official ASTC documentation](https://chromium.googlesource.com/external/github.com/ARM-software/astc-encoder/+/HEAD/Docs/FormatOverview.md):

    `ASTC at 8 bpt for LDR formats is comparable in quality to BC7 at 8 bpt.
    ASTC at 8 bpt for HDR formats is comparable in quality to BC6H at 8 bpt.`

Texture types: models texture, map, noise, other. To specify map you should simply
add prefix "map_" as well as for noises with "noise_" and for font with "font_",
which then will be treated in an appropriate way - rrr1 swizzle
for compression and rgba for decompression. If texture used my model,
it treated as a one of the model texture type:
- albedo (comp: rgba + astc flags ASTCENC_FLG_USE_ALPHA_WEIGHT
| ASTCENC_FLG_USE_PERCEPTUAL, decomp: rgba)
- metallic_roughness (comp: Gggb (because of GLTF 2.0 spec), decomp 0ra1);
- normal (comp: rrrg + astc flag ASTCENC_FLG_MAP_NORMAL, decomp raz1, where
z - recreated from first two component (reminder: vector normalized in 3d space,
so we can store only two and then use cross product));
- occlusion (comp: rrr1, decomp: rgba);
- emission (comp: rgb1, decomp: rgba).

For the rest of the textures we use this setup:
- ldr - comp:rgba + astc flags ASTCENC_FLG_USE_ALPHA_WEIGHT |
ASTCENC_FLG_USE_PERCEPTUAL, decomp: rgba
- hdr - comp: rgb1 (we don't need alpha for our purposes), decomp: rgba

---
### Audio Processing Issue:

#### 1 thread:
- slow (2s - 1 minute of the audio, what is too slow and any online converter is faster)

#### N threads:
- artifacts between threads data concatenation (looks like I don't
understand how libvorbis works OR I need to implement a MDCT, what is
also seems weird, because libvorbis probably should have been provided
at least "small support" for parallel processing)
- incorrect packet order (yes, ogg spec allows just to concatenate audio and
in some ogg-file players its playback supported, but not in all, what makes it
inconvenient to test)

#### Solution:
We **_don't_** process audio. (I simply don't know how to implement it by now,
need more information and experience with audio processing and ogg/vorbis in particular.)

I spent too much time ;D
I gave up ;C

---
### Branches:
* main - supported model & texture processing, but for audio assets - only copy
* dev - trying to integrate multithreading into audio processing

---
### Name:
it belongs to another project: game Faithful (pol-31/Faithful), so name appropriate.
(but still funny - like _faithful_ asset processor)
