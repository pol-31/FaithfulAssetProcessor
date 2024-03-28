#include "ModelProcessor.h"

#include <iostream>

#include "../../config/Paths.h"

bool TinygltfLoadTextureStub(tinygltf::Image *image, const int image_idx,
                             std::string *err, std::string *warn, int req_width,
                             int req_height, const unsigned char *bytes,
                             int size, void *user_data) {
  (void)image, (void)image_idx, (void)err, (void)warn, (void)req_width,
      (void)req_height, (void)bytes, (void)size, (void)user_data;
  /// main logic provided by ModelProcessor::DecompressTextures()
  return true;
}

ModelProcessor::ModelProcessor(
    TextureProcessor& texture_processor,
    ReplaceRequest& replace_request)
    : texture_processor_(texture_processor),
      replace_request_(replace_request) {
  /// force 4-channel loading, mandatory for astc
  loader_.SetPreserveImageChannels(true);
  /// while decompression we load images on our own because of ".astc" extension,
  /// which is not supported by GLTF 2.0 spec
  loader_.SetImageWriter(nullptr, nullptr);
}

void ModelProcessor::Encode(const std::filesystem::path& path) {
  std::string out_filename =
      (models_destination_path_ / path.filename().
                                  replace_extension(".gltf")).string();
  cur_model_path_ = path;
  loader_.SetImageLoader(&tinygltf::LoadImageData, nullptr);
  try {
    Read();
    CompressTextures();
    Write(out_filename);
    OptimizeModel(out_filename);
  } catch (const std::exception& e) {
    std::cerr << "Error at ModelProcessor::Encode: " << e.what() << std::endl;
  }
}
void ModelProcessor::Decode(const std::filesystem::path& path) {
  /// extension already ".astc"
  std::string out_filename =
      (models_destination_path_ / path.filename()).string();
  cur_model_path_ = path;
  loader_.SetImageLoader(TinygltfLoadTextureStub, nullptr);
  try {
    Read();
    DecompressTextures();
    Write(out_filename);
  } catch (const std::exception& e) {
    std::cerr << "Error at ModelProcessor::Decode: " << e.what() << std::endl;
  }
}

void ModelProcessor::Read() {
  model_ = std::make_unique<tinygltf::Model>();
  bool ret;
  if (cur_model_path_.extension() == ".glb") {
    ret = loader_.LoadBinaryFromFile(model_.get(), &error_string_,
                                          &warning_string_, cur_model_path_);
  } else if (cur_model_path_.extension() == ".gltf") {
    ret = loader_.LoadASCIIFromFile(model_.get(), &error_string_,
                                         &warning_string_, cur_model_path_);
  } else {
    throw std::invalid_argument("unsupported model file extension");
  }
  if (!warning_string_.empty()) {
    std::cerr << "Warning: " << warning_string_ << std::endl;
  }
  if (!error_string_.empty()) {
    throw std::runtime_error(error_string_);
  }
  if (!ret) {
    throw std::runtime_error("failed to load GLTF file");
  }
}

void ModelProcessor::Write(const std::string& destination) {
  /// it may seems logical to request replace before loading,
  /// but then textures related only to model and located externally
  /// to the model and also located inside the provided user's assets directory
  /// still would be processed. So we still need to open model to see
  /// what textures are used
  if (std::filesystem::exists(destination)) {
    std::string request{destination};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return;
    }
  }
  bool ret = loader_.WriteGltfSceneToFile(model_.get(), destination,
                                         false, false, true, false);
  if (!ret) {
    throw std::runtime_error("failed to write GLTF file");
  }
}

void ModelProcessor::CompressTextures() {
  for (std::size_t i = 0; i < model_->images.size(); ++i) {
    tinygltf::Image& image = model_->images[i];
    if (!image.uri.empty()) {
      processed_images_.insert((cur_model_path_.parent_path() / image.uri)
                                   .lexically_normal());
    }
    auto model_texture_config = ProvideEncodeTextureConfig(i);

    // forced 4 channels (see ctor)
    int total_len = image.width * image.height * 4;
    auto image_data = std::make_unique<uint8_t[]>(total_len);
    std::move(image.image.begin(), image.image.end(), image_data.get());
    image.image.clear();

    /// relative path (i.e. in the same directory)
    image.uri = model_texture_config.out_path.filename().string();
    image.mimeType.clear();
    image.name.clear();

    texture_processor_.Encode(
        model_texture_config.out_path, std::move(image_data),
        image.width, image.height, model_texture_config.category);
  }
}

void ModelProcessor::DecompressTextures() {
  for (std::size_t i = 0; i < model_->images.size(); ++i) {
    tinygltf::Image& image = model_->images[i];
    auto texture_path = (cur_model_path_.parent_path() / image.uri);
    processed_images_.insert(texture_path.string());

    auto model_texture_config = ProvideDecodeTextureConfig(
        texture_path.stem().string());

    image.uri = std::filesystem::path(image.uri)
                    .replace_extension(".png").string();
    texture_processor_.Decode(
        texture_path, model_texture_config.out_path,
        model_texture_config.category);
  }
}

ModelProcessor::ModelTextureConfig ModelProcessor::ProvideEncodeTextureConfig(
    int model_image_id) {
  auto out_path = (models_destination_path_ / cur_model_path_.stem()).string();
  TextureProcessor::TextureCategory category;
  /// our models consist only of one material, so we use it
  if (model_image_id == model_->materials[0].pbrMetallicRoughness
                            .metallicRoughnessTexture.index) {
    out_path += "_met_rough.astc";
    category = TextureProcessor::TextureCategory::kLdrGb;
  } else if (model_image_id == model_->materials[0].normalTexture.index) {
    out_path += "_normal.astc";
    category = TextureProcessor::TextureCategory::kLdrRgNmap;
  } else if (model_image_id == model_->materials[0].occlusionTexture.index) {
    out_path += "_occlusion.astc";
    category = TextureProcessor::TextureCategory::kLdrR;
  } else if (model_image_id == model_->materials[0].emissiveTexture.index) {
    out_path += "_emissive.astc";
    category = TextureProcessor::TextureCategory::kLdrRgb;
  } else { // albedo (material.pbrMetallicRoughness.baseColorTexture.index)
    out_path += "_albedo.astc";
    category = TextureProcessor::TextureCategory::kLdrRgba;
  }
  return {out_path, category};
}

ModelProcessor::ModelTextureConfig ModelProcessor::ProvideDecodeTextureConfig(
    std::string_view path) {
  auto out_path = (models_destination_path_ / path).replace_extension(".png")
                      .string();
  TextureProcessor::TextureCategory category;
  if (path.ends_with("_met_rough")) {
    category = TextureProcessor::TextureCategory::kLdrGb;
  } else if (path.ends_with("_normal")) {
    category = TextureProcessor::TextureCategory::kLdrRgNmap;
  } else if (path.ends_with("_occlusion")) {
    category = TextureProcessor::TextureCategory::kLdrR;
  } else if (path.ends_with("_emissive")) {
    category = TextureProcessor::TextureCategory::kLdrRgb;
  } else { // probably for "_albedo"
    category = TextureProcessor::TextureCategory::kLdrRgba;
  }
  return {out_path, category};
}

void ModelProcessor::OptimizeModel(const std::string& destination) {
  std::string command;
  command.reserve(std::strlen(FAITHFUL_ASSET_PROCESSOR_GLTFPACK_PATH) +
                  destination.size() * 2 + 17); // 17 for flags, whitespaces
  command = FAITHFUL_ASSET_PROCESSOR_GLTFPACK_PATH;
  /// no changes for textures, no quantization
  /// (requires extension, while tinygltf don't support it)
  command += " -tr -noq -i ";
  command += destination;
  command += " -o ";
  command += destination;
  if (std::system(command.c_str()) != 0) {
    throw std::runtime_error("unable to optimize model");
  }
}

void ModelProcessor::SetDestinationDirectory(
    const std::filesystem::path& path) {
  models_destination_path_ = path / "models";
  std::filesystem::create_directories(models_destination_path_);
}
