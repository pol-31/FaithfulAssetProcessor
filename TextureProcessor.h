#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_TEXTUREPROCESSOR_H
#define FAITHFUL_UTILS_ASSETPROCESSOR_TEXTUREPROCESSOR_H

#include <filesystem>
#include <memory>
#include <string>

#include <astc-encoder/Source/astcenc.h>

#include "AssetLoadingThreadPool.h"
#include "ReplaceRequest.h"

struct AstcHeader {
  uint8_t magic[4]; // format identifier
  uint8_t block_x;
  uint8_t block_y;
  uint8_t block_z;
  uint8_t dim_x[3];
  uint8_t dim_y[3];
  uint8_t dim_z[3];
};

class TextureProcessor {
 public:
  enum class TextureCategory {
    kLdrR,
    kLdrGb,
    kLdrRgb,
    kLdrRgba,
    kLdrRgNmap,
    kHdrRgb
  };

  TextureProcessor() = delete;
  TextureProcessor(AssetLoadingThreadPool& thread_pool,
                   ReplaceRequest& replace_request);

  /// non-assignable because of member reference
  TextureProcessor(const TextureProcessor&) = delete;
  TextureProcessor& operator=(const TextureProcessor&) = delete;

  TextureProcessor(TextureProcessor&&) = default;
  TextureProcessor& operator=(TextureProcessor&&) = delete;

  ~TextureProcessor();

  void Encode(const std::filesystem::path& path);

  /// used by ModelProcessor
  void Encode(const std::filesystem::path& out_path,
              std::unique_ptr<uint8_t[]> image_data,
              int width, int height,
              TextureCategory category);

  void Decode(const std::filesystem::path& path);

  /// used by ModelProcessor
  void Decode(const std::filesystem::path& in_path,
              const std::filesystem::path& out_path,
              TextureCategory category);

  void SetDestinationDirectory(const std::filesystem::path& path);

 private:
  struct TextureConfig {
    std::string out_path;
    astcenc_swizzle swizzle;
    astcenc_context*& context;
    TextureCategory category;
    astcenc_type type;
  };

  void InitContexts();
  void DeInitContexts();

  void InitContext(astcenc_config config, astcenc_context*& context);

  bool MakeReplaceRequest(const std::filesystem::path& filename);

  void DecodeImpl(const std::filesystem::path& path,
                  TextureConfig texture_config);

  void WriteEncodedData(std::string filename, unsigned int image_x,
                        unsigned int image_y, int comp_data_size,
                        std::unique_ptr<uint8_t[]> comp_data);
  void WriteDecodedData(std::string filename, unsigned int image_x,
                        unsigned int image_y, TextureCategory category,
                        std::unique_ptr<uint8_t[]> image_data);

  int CalculateCompLen(int image_x, int image_y);

  TextureConfig ProvideEncodeTextureConfig(const std::filesystem::path& path);
  TextureConfig ProvideEncodeTextureConfig(TextureCategory category);
  TextureConfig ProvideDecodeTextureConfig(const std::filesystem::path& path);
  TextureConfig ProvideDecodeTextureConfig(TextureCategory category);

  bool ReadAstcFile(const std::string& path, int& width, int& height,
                    int& comp_len, std::unique_ptr<uint8_t[]>& comp_data);

  bool HasMapPrefix(const std::filesystem::path& path);
  bool HasNoisePrefix(const std::filesystem::path& path);
  bool HasFontPrefix(const std::filesystem::path& path);
  bool HasHdrPrefix(const std::filesystem::path& path);
  bool HasHdrExtension(const std::filesystem::path& path);

  AssetLoadingThreadPool& thread_pool_;
  ReplaceRequest& replace_request_;

  astcenc_context* context_ldr_ = nullptr;
  astcenc_context* context_hdr_ = nullptr;
  astcenc_context* context_ldr_normal_ = nullptr;
  astcenc_context* context_ldr_alpha_perceptual_ = nullptr;

  std::filesystem::path default_destination_path_;
  std::filesystem::path maps_destination_path_;
  std::filesystem::path noises_destination_path_;
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_TEXTUREPROCESSOR_H
