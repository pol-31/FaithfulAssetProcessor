#include "AssetProcessor.h"

#include <algorithm>

AssetProcessor::AssetProcessor(int thread_count)
    : thread_pool_(std::max(1, thread_count)),
      replace_request_(),
      audio_processor_(replace_request_),
      texture_processor_(thread_pool_, replace_request_),
      model_processor_(texture_processor_, replace_request_) {}

void AssetProcessor::Process(
    const std::filesystem::path& destination,
    const std::filesystem::path& source,
    bool encode) {
  // important for debugging to reuse ReplaceRequest
  replace_request_.ClearFlags();

  if (std::filesystem::exists(destination / "models") ||
      std::filesystem::exists(destination / "music") ||
      std::filesystem::exists(destination / "sounds") ||
      std::filesystem::exists(destination / "noises") ||
      std::filesystem::exists(destination / "maps")) {
    if (!replace_request_(
            "Needed hierarchy {models, music, sounds, noises, maps} "
            "inside the destination path already exist."
            "\nDo you want to continue?"
            "\n(there may be quite a lot replace requests)")) {
      return;
    }
  }
  AssetsAnalyzer assets_analyzer(source, encode);

  audio_processor_.SetDestinationDirectory(destination.string());
  model_processor_.SetDestinationDirectory(destination.string());
  texture_processor_.SetDestinationDirectory(destination.string());

  thread_pool_.Run();
  if (encode) {
    EncodeAssets(assets_analyzer);
  } else {
    DecodeAssets(assets_analyzer);
  }
  thread_pool_.Stop();
}

void AssetProcessor::EncodeAssets(AssetsAnalyzer& assets_analyzer) {
  /// for music(.ogg) & sounds(.wav) just copy
  auto music_to_process = assets_analyzer.GetMusicToProcess();
  for (const auto& path : music_to_process) {
    std::cout << "--> encoding: " << path << std::endl;
    audio_processor_.EncodeMusic(path);
  }
  auto sounds_to_process = assets_analyzer.GetSoundsToProcess();
  for (const auto& path : sounds_to_process) {
    std::cout << "--> encoding: " << path << std::endl;
    audio_processor_.EncodeSound(path);
  }

  /// models always before textures to not to process models textures twice,
  /// so then we just remove already processed (see below in this function)
  auto& models_to_process = assets_analyzer.GetModelsToProcess();
  for (const auto& path : models_to_process) {
    std::cout << "--> encoding: " << path << std::endl;
    model_processor_.Encode(path);
  }

  /// remove already processed by model_processor_
  /// (if user_source_path had models textures located beyond the gltf file,
  /// they will be processed twice - by model_processor_ and texture_processor_,
  /// so we avoiding this)
  auto all_textures_to_process = assets_analyzer.GetTexturesToProcess();
  auto processed_textures = model_processor_.GetProcessedTextures();
  std::vector<std::string> textures_to_process;
  std::set_difference(
      all_textures_to_process.begin(), all_textures_to_process.end(),
      processed_textures.begin(), processed_textures.end(),
      std::back_inserter(textures_to_process));

  for (const auto& path : textures_to_process) {
    std::cout << "--> encoding: " << path << std::endl;
    texture_processor_.Encode(path);
  }
}

void AssetProcessor::DecodeAssets(AssetsAnalyzer& assets_analyzer) {
  /// for music(.ogg) & sounds(.wav) just copy
  auto music_to_process = assets_analyzer.GetMusicToProcess();
  for (const auto& path : music_to_process) {
    std::cout << "--> decoding: " << path << std::endl;
    audio_processor_.DecodeMusic(path);
  }
  auto sounds_to_process = assets_analyzer.GetSoundsToProcess();
  for (const auto& path : sounds_to_process) {
    std::cout << "--> decoding: " << path << std::endl;
    audio_processor_.DecodeSound(path);
  }

  /// it also handles models textures (textures located inside the "models/")
  auto& models_to_process = assets_analyzer.GetModelsToProcess();
  for (const auto& path : models_to_process) {
    std::cout << "--> decoding: " << path << std::endl;
    model_processor_.Decode(path);
  }

  /// remove already processed by model_processor_
  auto all_textures_to_process = assets_analyzer.GetTexturesToProcess();
  auto processed_textures = model_processor_.GetProcessedTextures();
  std::vector<std::string> textures_to_process;
  std::set_difference(
      all_textures_to_process.begin(), all_textures_to_process.end(),
      processed_textures.begin(), processed_textures.end(),
      std::back_inserter(textures_to_process));

  for (const auto& path : textures_to_process) {
    std::cout << "--> decoding: " << path << std::endl;
    texture_processor_.Decode(path);
  }
}
