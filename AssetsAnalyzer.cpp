#include "AssetsAnalyzer.h"

#include <exception>
#include <iostream>

#include "../../config/AssetFormats.h"

AssetsAnalyzer::AssetsAnalyzer(
    const std::filesystem::path& path, bool encode)
    : encode_(encode) {
  AnalyzePath(path);
}

void AssetsAnalyzer::AnalyzePath(const std::filesystem::path& path) {
  if (std::filesystem::is_regular_file(path)) {
    AddEntry(path);
  } else if (std::filesystem::is_directory(path)) {
    AnalyzeDir(path);
  } else {
    throw std::invalid_argument("incorrect path");
  }
}

void AssetsAnalyzer::AnalyzeDir(const std::filesystem::path& path) {
  for (const std::filesystem::path& entry :
       std::filesystem::directory_iterator(path)) {
    if (std::filesystem::is_regular_file(entry)) {
      AddEntry(entry);
    } else if (std::filesystem::is_directory(entry)) {
      AnalyzeDir(entry);
    }
  }
}

void AssetsAnalyzer::AddEntry(const std::filesystem::path& path) {
  auto category = DeduceAssetCategory(path);
  switch (category) {
    case AssetCategory::kMusic:
      AddEntryImpl(path, music_to_process_);
      break;
    case AssetCategory::kSound:
      AddEntryImpl(path, sounds_to_process_);
      break;
    case AssetCategory::kModel:
      AddEntryImpl(path, models_to_process_);
      break;
    case AssetCategory::kTexture:
      AddEntryImpl(path, textures_to_process_);
      break;
    case AssetCategory::kUnknown:
      break;
  }
}

void AssetsAnalyzer::AddEntryImpl(const std::filesystem::path& new_asset,
                                  std::set<std::filesystem::path>& assets) {
  auto founded = assets.find(new_asset);
  if (founded != assets.end()) {
    std::cerr << "Source path has at least two files with the same name:\n"
              << founded->string() << "\n" << new_asset.string()
              << "\nOnly first will be processed"
              << "\nRename second and run program again to process second"
              << std::endl;
    return;
  }
  assets.insert(new_asset.string());
}

AssetsAnalyzer::AssetCategory AssetsAnalyzer::DeduceAssetCategory(
    const std::filesystem::path& path) {
  if (encode_) { // order from more_supported_num to less
    for (const auto& extension : faithful::config::kModelCompFormats) {
      if (extension == path.extension()) {
        return AssetCategory::kModel;
      }
    }
    if (path.extension() == ".ogg") {
      return AssetCategory::kMusic;
    }
    if (path.extension() == ".wav") {
      return AssetCategory::kSound;
    }
    for (const auto& extension : faithful::config::kTexCompFormats) {
      if (extension == path.extension()) {
        return AssetCategory::kTexture;
      }
    }
  } else { // order from more_checks to less
    if (path.extension() == ".gltf") {
      return AssetCategory::kModel;
    } else if (path.extension() == ".astc") {
      return AssetCategory::kTexture;
    } else if (path.extension() == ".ogg") {
      return AssetCategory::kMusic;
    } else if (path.extension() == ".wav") {
      return AssetCategory::kSound;
    }
  }
  return AssetCategory::kUnknown;
}
