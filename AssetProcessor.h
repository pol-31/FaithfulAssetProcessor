#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_ASSETPROCESSOR_H
#define FAITHFUL_UTILS_ASSETPROCESSOR_ASSETPROCESSOR_H

#include <filesystem>
#include <thread>

#include "AssetLoadingThreadPool.h"
#include "AssetsAnalyzer.h"
#include "ReplaceRequest.h"

#include "AudioProcessor.h"
#include "ModelProcessor.h"
#include "TextureProcessor.h"

class AssetProcessor {
 public:
  AssetProcessor(int thread_count = std::thread::hardware_concurrency());

  /// neither movable nor copyable because of AssetLoadingThreadPool
  AssetProcessor(const AssetProcessor& other) = delete;
  AssetProcessor& operator=(const AssetProcessor& other) = delete;

  AssetProcessor(AssetProcessor&& other) = delete;
  AssetProcessor& operator=(AssetProcessor&& other) = delete;

  void Process(const std::filesystem::path& destination,
               const std::filesystem::path& source,
               bool encode);

 private:
  void EncodeAssets(AssetsAnalyzer& assets_analyzer);
  void DecodeAssets(AssetsAnalyzer& assets_analyzer);

  AssetLoadingThreadPool thread_pool_;
  ReplaceRequest replace_request_;
  AudioProcessor audio_processor_;
  TextureProcessor texture_processor_;
  ModelProcessor model_processor_;
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_ASSETPROCESSOR_H
