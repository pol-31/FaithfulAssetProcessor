#include "TextureProcessor.h"

#include <exception>
#include <fstream>
#include <iostream>

#include <stb_image.h>
#include <stb_image_write.h>

#include "../../config/AssetFormats.h"

TextureProcessor::TextureProcessor(
    AssetLoadingThreadPool& thread_pool,
    ReplaceRequest& replace_request)
    : thread_pool_(thread_pool),
      replace_request_(replace_request) {
  InitContexts();
}

TextureProcessor::~TextureProcessor() {
  DeInitContexts();
}

void TextureProcessor::InitContexts() {
  InitContext(faithful::config::kTextureConfigLdr, context_ldr_);
  InitContext(faithful::config::kTextureConfigHdr, context_hdr_);
  InitContext(faithful::config::kTextureConfigLdrNormal,
              context_ldr_normal_);
  InitContext(faithful::config::kTextureConfigLdrAlphaPerceptual,
              context_ldr_alpha_perceptual_);
}

void TextureProcessor::InitContext(astcenc_config config,
                                   astcenc_context*& context) {
  auto config_copy = config;
  astcenc_error status = astcenc_config_init(
      config.profile, config.block_x,
      config.block_y, config.block_z,
      faithful::config::kTexCompQuality, config.flags, &config_copy);
  if (status != ASTCENC_SUCCESS) {
    std::string error_string{"TextureProcessor::InitContext astcenc_config_init:\n"};
    error_string += astcenc_get_error_string(status);
    throw std::runtime_error(std::move(error_string));
  }
  status = astcenc_context_alloc(
      &config_copy, thread_pool_.GetThreadNumber(), &context);
  if (status != ASTCENC_SUCCESS) {
    std::string error_string{"TextureProcessor::InitContext astcenc_context_alloc:\n"};
    error_string += astcenc_get_error_string(status);
    throw std::runtime_error(std::move(error_string));
  }
}

void TextureProcessor::DeInitContexts() {
  astcenc_context_free(context_ldr_);
  astcenc_context_free(context_hdr_);
  astcenc_context_free(context_ldr_normal_);
  astcenc_context_free(context_ldr_alpha_perceptual_);
}

void TextureProcessor::Encode(const std::filesystem::path& path) {
  auto texture_config = ProvideEncodeTextureConfig(path);
  if (!MakeReplaceRequest(texture_config.out_path)) {
    return;
  }
  astcenc_compress_reset(texture_config.context);
  /// We add the prefix "hdr_" to the file {actual_name}.hdr to distinguish
  /// between LDR and HDR textures during decompression (ASTC header doesn't
  /// provide this information). However, it's possible that the user has
  /// already added this prefix, and if they added it to an LDR file, it
  /// will be decoded as an HDR file during decompression because of the prefix.
  /// Therefore, we're skipping it and politely asking the user to rename it.
  if (texture_config.category != TextureCategory::kHdrRgb &&
      HasHdrPrefix(path)) {
    std::cerr
        << "Skipped: \"" << path
        << "\" because it has the prefix \"hdr_\" for an LDR texture."
        << "\nPlease rename it (\"hdr_\" is used in decompression as a hint)."
        << std::endl;
    return;
  }

  int image_x, image_y, image_c;

  /// RAII
  std::unique_ptr<uint8_t[]> image_data_ptr_uint8;
  std::unique_ptr<float[]> image_data_ptr_float;

  /// astcenc_image requires l-value ref, so std::unique_ptr::get() doesn't work
  float* image_data_ptr_float_ptr;
  uint8_t* image_data_ptr_uint8_ptr;

  void** image_data_ptr;

  /// force 4 component (astc requirement)
  if (texture_config.category != TextureCategory::kHdrRgb) {
    auto image_data = static_cast<uint8_t*>(stbi_load(
        path.string().c_str(),&image_x, &image_y, &image_c, 4));
    if (!image_data) {
      std::cerr << "Error: stb_image texture loading failed: " << path
                << std::endl;
      return;
    }
    image_data_ptr_uint8 = std::unique_ptr<uint8_t[]>(image_data);
    image_data_ptr_uint8_ptr = image_data_ptr_uint8.get();
    image_data_ptr = reinterpret_cast<void**>(&image_data_ptr_uint8_ptr);
  } else {
    float* image_data = stbi_loadf(path.string().c_str(),
                                 &image_x, &image_y, &image_c, 4);
    if (!image_data) {
      std::cerr << "Error: stb_image texture loading failed: " << path
                << std::endl;
      return;
    }
    image_data_ptr_float = std::unique_ptr<float[]>(image_data);
    image_data_ptr_float_ptr = image_data_ptr_float.get();
    image_data_ptr = reinterpret_cast<void**>(&image_data_ptr_float_ptr);
  }

  int comp_len = CalculateCompLen(image_x, image_y);
  auto comp_data = std::make_unique<uint8_t[]>(comp_len);

  astcenc_image image {
      static_cast<unsigned int>(image_x), static_cast<unsigned int>(image_y),
      1, texture_config.type, image_data_ptr
  };
  // no need to make it atomic, only "fail"-thread write;
  // order doesn't matter, need only true/false
  bool encode_success = true;
  thread_pool_.Execute(
      [&, comp_len, comp_data_get = comp_data.get()](int thread_id) {
        astcenc_error status = astcenc_compress_image(
            texture_config.context, &image, &texture_config.swizzle,
            comp_data_get, comp_len, thread_id);
        if (status != ASTCENC_SUCCESS) {
          encode_success = false;
        }
      });

  if (!encode_success) {
    std::cerr << "Error: texture compression failed for: "
              << path << std::endl;
    return;
  }

  WriteEncodedData(texture_config.out_path, image_x, image_y,
                   comp_len, std::move(comp_data));
}

void TextureProcessor::Encode(const std::filesystem::path& out_path,
                              std::unique_ptr<uint8_t[]> image_data,
                              int width, int height,
                              TextureCategory category) {
  auto texture_config = ProvideEncodeTextureConfig(category);
  if (!MakeReplaceRequest(out_path)) {
    return;
  }
  astcenc_compress_reset(texture_config.context);

  int comp_len = CalculateCompLen(width, height);
  auto comp_data = std::make_unique<uint8_t[]>(comp_len);

  /// astcenc_image requires l-value ref, so std::unique_ptr::get() doesn't work
  auto data_ptr = reinterpret_cast<void*>(image_data.get());

  astcenc_image image {
      static_cast<unsigned int>(width), static_cast<unsigned int>(height),
      1, texture_config.type, reinterpret_cast<void**>(&data_ptr)
  };

  // no need to make it atomic, only "fail"-thread write;
  // order doesn't matter, need only true/false
  bool encode_success = true;
  thread_pool_.Execute(
      [&, comp_len, comp_data_get = comp_data.get()](int thread_id) {
        astcenc_error status = astcenc_compress_image(
            texture_config.context, &image, &texture_config.swizzle,
            comp_data_get, comp_len, thread_id);
        if (status != ASTCENC_SUCCESS) {
          encode_success = false;
        }
      });

  if (!encode_success) {
    std::cerr << "Error: texture compression failed for: "
              << out_path << std::endl;
    return;
  }

  WriteEncodedData(out_path, width, height, comp_len, std::move(comp_data));
}

void TextureProcessor::WriteEncodedData(
    std::string filename, unsigned int image_x, unsigned int image_y,
    int comp_data_size, std::unique_ptr<uint8_t[]> comp_data) {
  std::ofstream out_file(filename, std::ios::binary);
  if (!out_file.is_open()) {
    std::cerr << "Error: failed to create file for encoded data" << std::endl;
    return;
  }
  AstcHeader header;
  header.magic[0] = 0x13;
  header.magic[1] = 0xAB;
  header.magic[2] = 0xA1;
  header.magic[3] = 0x5C;

  header.block_x = faithful::config::kTexCompBlockX;
  header.block_y = faithful::config::kTexCompBlockY;
  header.block_z = 1;

  header.dim_x[2] = static_cast<uint8_t>(image_x >> 16);
  header.dim_x[1] = static_cast<uint8_t>(image_x >> 8);
  header.dim_x[0] = static_cast<uint8_t>(image_x);

  header.dim_y[2] = static_cast<uint8_t>(image_y >> 16);
  header.dim_y[1] = static_cast<uint8_t>(image_y >> 8);
  header.dim_y[0] = static_cast<uint8_t>(image_y);

  header.dim_z[0] = 1;
  header.dim_z[1] = 0;
  header.dim_z[2] = 0;

  out_file.write(reinterpret_cast<const char*>(&header), sizeof(AstcHeader));
  out_file.write(reinterpret_cast<const char*>(comp_data.get()), comp_data_size);
}

void TextureProcessor::Decode(const std::filesystem::path& path) {
  auto texture_config = ProvideDecodeTextureConfig(path);
  DecodeImpl(path, std::move(texture_config));
}

void TextureProcessor::Decode(const std::filesystem::path& in_path,
                              const std::filesystem::path& out_path,
                              TextureCategory category) {
  auto texture_config = ProvideDecodeTextureConfig(category);
  texture_config.out_path = out_path;
  DecodeImpl(in_path, std::move(texture_config));
}

void TextureProcessor::DecodeImpl(
    const std::filesystem::path& path,
    TextureProcessor::TextureConfig texture_config) {
  if (!MakeReplaceRequest(texture_config.out_path)) {
    return;
  }
  astcenc_decompress_reset(texture_config.context);

  int image_x, image_y, comp_len;
  std::unique_ptr<uint8_t[]> comp_data;
  if (!ReadAstcFile(path, image_x, image_y, comp_len, comp_data)) {
    return;
  }

  // for float (hdr) just x4 size, anyway casting to void* further
  std::unique_ptr<uint8_t[]> image_data;
  /// 4 channels
  if (texture_config.category == TextureCategory::kHdrRgb) {
    /// *4 because of float32
    image_data = std::make_unique<uint8_t[]>(image_x * image_y * 4 * 4);
  } else {
    image_data = std::make_unique<uint8_t[]>(image_x * image_y * 4);
  }

  astcenc_image image {
      static_cast<unsigned int>(image_x), static_cast<unsigned int>(image_y),
      1, texture_config.type, reinterpret_cast<void**>(&image_data)
  };

  // no need to make it atomic, only "fail"-thread write;
  // order doesn't matter, need only true/false
  bool decode_success = true;
  thread_pool_.Execute(
      [&, comp_len, comp_data_get = comp_data.get()](int thread_id) {
        astcenc_error status = astcenc_decompress_image(
            texture_config.context, comp_data_get, comp_len,
            &image, &texture_config.swizzle, thread_id);
        if (status != ASTCENC_SUCCESS) {
          decode_success = false;
        }
      });

  if (!decode_success) {
    std::cerr << "Error: texture decompression failed for: "
              << path << std::endl;
    return;
  }

  WriteDecodedData(texture_config.out_path, image_x, image_y,
                   texture_config.category, std::move(image_data));
}

void TextureProcessor::WriteDecodedData(
    std::string filename, unsigned int image_x, unsigned int image_y,
    TextureCategory category, std::unique_ptr<uint8_t[]> image_data) {
  if (category != TextureCategory::kHdrRgb) {
    if (!stbi_write_png(filename.c_str(), image_x, image_y, 4,
                        image_data.get(), 4 * image_x)) {
      std::cerr << "Error: stb_image_write failed to save texture" << std::endl;
    }
  } else {
    if (!stbi_write_hdr(filename.c_str(), image_x, image_y, 4,
                        reinterpret_cast<const float*>(image_data.get()))) {
      std::cerr << "Error: stb_image_write failed to save texture" << std::endl;
    }
  }
}

int TextureProcessor::CalculateCompLen(int image_x, int image_y) {
  int block_count_x =
      (image_x + faithful::config::kTexCompBlockX - 1)
      / faithful::config::kTexCompBlockX;
  int block_count_y =
      (image_y + faithful::config::kTexCompBlockY - 1)
      / faithful::config::kTexCompBlockY;
  return block_count_x * block_count_y * 16;
}

bool TextureProcessor::ReadAstcFile(const std::string& path, int& width,
                                    int& height, int& comp_len,
                                    std::unique_ptr<uint8_t[]>& comp_data) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: texture loading failed: " << path << std::endl;
    return false;
  }
  AstcHeader header;
  file.read(reinterpret_cast<char*>(&header), sizeof(AstcHeader));

  if (header.magic[0] != 0x13 || header.magic[1] != 0xAB ||
      header.magic[2] != 0xA1 || header.magic[3] != 0x5C) {
    std::cerr << "Error: invalid ASTC file (metadata): " << path << std::endl;
    return false;
  }

  if ((header.dim_z[0] | header.dim_z[1] << 8 | header.dim_z[2]) != 1) {
    std::cerr << "Error: only 2d textures supported: " << path << std::endl;
    return false;
  }

  if (header.block_x != faithful::config::kTexCompBlockX ||
      header.block_y != faithful::config::kTexCompBlockY ||
      header.block_z != 1) {
    std::cerr << "Error: file compressed by another astcenc is not supported: "
              << path << std::endl;
    return false;
  }

  // we don't care about block sizes, because currently all textures compressed
  // with the same configs (see Faithful/config/AssetFormats.h)
  width = header.dim_x[0] | header.dim_x[1] << 8 | header.dim_x[2] << 16;
  height = header.dim_y[0] | header.dim_y[1] << 8 | header.dim_y[2] << 16;

  comp_len = CalculateCompLen(width, height);
  comp_data = std::make_unique<uint8_t[]>(comp_len);
  file.read(reinterpret_cast<char*>(comp_data.get()), comp_len);
  return true;
}

void TextureProcessor::SetDestinationDirectory(
    const std::filesystem::path& path) {
  default_destination_path_ = path;
  maps_destination_path_ = path / "maps";
  noises_destination_path_ = path / "noises";
  std::filesystem::create_directories(default_destination_path_);
  std::filesystem::create_directories(maps_destination_path_);
  std::filesystem::create_directories(noises_destination_path_);
}

TextureProcessor::TextureConfig TextureProcessor::ProvideEncodeTextureConfig(
    const std::filesystem::path& path) {
  std::string out_filename = path.filename().replace_extension(".astc").string();
  if (HasMapPrefix(path)) {
    return {
        (maps_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasNoisePrefix(path)) {
    return {
        (noises_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasFontPrefix(path)) {
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasHdrExtension(path)) {
    out_filename = "hdr_" + out_filename;
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRgb1,
        context_hdr_,
        TextureCategory::kHdrRgb,
        faithful::config::kTexHdrDataType
    };
  } else {
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRgba,
        context_ldr_alpha_perceptual_,
        TextureCategory::kLdrRgba,
        faithful::config::kTexLdrDataType
    };
  }
}

TextureProcessor::TextureConfig TextureProcessor::ProvideEncodeTextureConfig(
    TextureCategory category) {
  switch (category) {
    case TextureCategory::kLdrR:
      return {
          "",
          faithful::config::kTextureSwizzleRrr1,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrGb:
      return {
          "",
          faithful::config::kTextureSwizzleGggb,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgb:
      return {
          "",
          faithful::config::kTextureSwizzleRgb1,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgba:
      return {
          "",
          faithful::config::kTextureSwizzleRgba,
          context_ldr_alpha_perceptual_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgNmap:
      return {
          "",
          faithful::config::kTextureSwizzleRrrg,
          context_ldr_normal_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kHdrRgb:
      return {
          "",
          faithful::config::kTextureSwizzleRgb1,
          context_hdr_,
          category,
          faithful::config::kTexHdrDataType
      };
  }
}

TextureProcessor::TextureConfig TextureProcessor::ProvideDecodeTextureConfig(
    const std::filesystem::path& path) {
  std::string out_filename = path.filename().replace_extension(".png").string();
  if (HasMapPrefix(path)) {
    return {
        (maps_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasNoisePrefix(path)) {
    return {
        (noises_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasFontPrefix(path)) {
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRrr1,
        context_ldr_,
        TextureCategory::kLdrR,
        faithful::config::kTexLdrDataType
    };
  } else if (HasHdrPrefix(path)) {
    out_filename.substr(4); // remove prefix hdr_
    out_filename.replace(out_filename.size() - 3, 3, "hdr"); // replace extension
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRgb1,
        context_hdr_,
        TextureCategory::kHdrRgb,
        faithful::config::kTexHdrDataType
    };
  } else {
    return {
        (default_destination_path_ / std::move(out_filename)).string(),
        faithful::config::kTextureSwizzleRgba,
        context_ldr_,
        TextureCategory::kLdrRgba,
        faithful::config::kTexLdrDataType
    };
  }
}

TextureProcessor::TextureConfig TextureProcessor::ProvideDecodeTextureConfig(
    TextureCategory category) {
  switch (category) {
    case TextureCategory::kLdrR:
      return {
          "",
          faithful::config::kTextureSwizzleRgba,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrGb:
      return {
          "",
          faithful::config::kTextureSwizzle0ra1,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgb:
      return {
          "",
          faithful::config::kTextureSwizzleRgba,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgba:
      return {
          "",
          faithful::config::kTextureSwizzleRgba,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kLdrRgNmap:
      return {
          "",
          faithful::config::kTextureSwizzleRaz1,
          context_ldr_,
          category,
          faithful::config::kTexLdrDataType
      };
    case TextureCategory::kHdrRgb:
      return {
          "",
          faithful::config::kTextureSwizzleRgba,
          context_hdr_,
          category,
          faithful::config::kTexHdrDataType
      };
  }
}

bool TextureProcessor::HasMapPrefix(const std::filesystem::path& path) {
  auto path_string = path.filename().string();
  return path_string.size() >= 4 && path_string.substr(0, 4) == "map_";
}

bool TextureProcessor::HasNoisePrefix(const std::filesystem::path& path) {
  auto path_string = path.filename().string();
  return path_string.size() >= 6 && path_string.substr(0, 6) == "noise_";
}

bool TextureProcessor::HasFontPrefix(const std::filesystem::path& path) {
  auto path_string = path.filename().string();
  return path_string.size() >= 5 && path_string.substr(0, 5) == "font_";
}

bool TextureProcessor::HasHdrPrefix(const std::filesystem::path& path) {
  auto path_string = path.filename().string();
  return path_string.size() >= 4 && path_string.substr(0, 4) == "hdr_";
}

bool TextureProcessor::HasHdrExtension(const std::filesystem::path& path) {
  return path.extension() == ".hdr" || path.extension() == ".HDR";
}

bool TextureProcessor::MakeReplaceRequest(
    const std::filesystem::path& filename) {
  if (std::filesystem::exists(filename)) {
    std::string request{filename};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return false;
    }
  }
  return true;
}
