#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_MODELPROCESSOR_H
#define FAITHFUL_UTILS_ASSETPROCESSOR_MODELPROCESSOR_H

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <tiny_gltf.h>

#include "TextureProcessor.h"
#include "ReplaceRequest.h"

/// while decompression we load images on our own because of ".astc" extension,
/// which is not supported by GLTF 2.0 spec
bool TinygltfLoadTextureStub(tinygltf::Image *image, const int image_idx,
                             std::string *err, std::string *warn, int req_width,
                             int req_height, const unsigned char *bytes,
                             int size, void *user_data);

class ModelProcessor {
 public:
  ModelProcessor() = delete;
  ModelProcessor(TextureProcessor& texture_processor,
                 ReplaceRequest& replace_request);

  /// only move-constructable because of std::unique_ptr and member reference
  ModelProcessor(const ModelProcessor&) = delete;
  ModelProcessor& operator=(const ModelProcessor&) = delete;

  ModelProcessor(ModelProcessor&&) = default;
  ModelProcessor& operator=(ModelProcessor&&) = delete;

  void Encode(const std::filesystem::path& path);
  void Decode(const std::filesystem::path& path);

  void SetDestinationDirectory(const std::filesystem::path& path);

  const std::set<std::string>& GetProcessedTextures() const {
    return processed_images_;
  }

  private:
   struct ModelTextureConfig {
     std::filesystem::path out_path;
     TextureProcessor::TextureCategory category;
   };
  void Read();
  void Write(const std::string& destination);

  void CompressTextures();
  void DecompressTextures();

  ModelTextureConfig ProvideEncodeTextureConfig(int model_image_id);

  /// filename stem as an input parameter
  ModelTextureConfig ProvideDecodeTextureConfig(std::string_view path);

  void OptimizeModel(const std::string& path);

  /// in case if texture embedded, we directly ask texture processor to process
  TextureProcessor& texture_processor_;

  ReplaceRequest& replace_request_;

  std::set<std::string> processed_images_;

  std::filesystem::path cur_model_path_;
  std::unique_ptr<tinygltf::Model> model_;
  tinygltf::TinyGLTF loader_;
  std::string error_string_;
  std::string warning_string_;
  std::filesystem::path models_destination_path_;
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_MODELPROCESSOR_H
