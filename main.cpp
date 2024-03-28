/** AssetProcessor converts assets into formats used by Faithful game internally:
 * - textures: .astc
 * - 3D models: .gltf
 *
 * See compress config in Faithful/config/AssetFormats.h
 *
 * Supported:
 * - textures(images): bmp, hdr, HDR, jpeg, jpg, pgm, png, ppm, psd, tga;
 *     (for more information see faithful/external/stb/stb_image.h)
 * - 3D models: glb, gltf
 *
 * for audio, we just copy .wav to /sounds/ and .ogg to /music/
 * */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "AssetProcessor.h"
#include "../../config/AssetFormats.h"

/// by default all assets have such info: id;name;
/// except models: id;name;type;sound_ids;
void UpdateAssetsInfo(const std::filesystem::path& path,
                      bool is_models_dir = false) {
  if (!std::filesystem::exists(path)) {
    return;
  }
  std::filesystem::path assets_info_path{path / "info.txt"};
  std::set<std::string> all_assets;
  for (auto&& entry : std::filesystem::directory_iterator(path)) {
    if (entry.is_regular_file()) {
      if (is_models_dir) {
        if (entry.path().extension() != ".gltf") {
          continue;
        }
      }
      all_assets.insert(entry.path().filename().string());
    }
  }
  int last_id{0};
  std::ofstream new_info_file;
  std::vector<std::string> new_assets;
  if (std::filesystem::exists(assets_info_path)) {
    std::set<std::string> old_assets;
    std::ifstream old_info_file(assets_info_path.c_str());
    std::string line;
    std::string field;
    while (std::getline(old_info_file, line)) {
      std::stringstream line_stream{line};
      std::getline(line_stream, field, ';');
      last_id = std::max(last_id, std::stoi(field));
      std::getline(line_stream, field, ';');
      old_assets.insert(field);
    }
    std::set_difference(all_assets.begin(), all_assets.end(),
                        old_assets.begin(), old_assets.end(),
                        std::back_inserter(new_assets));
    if (new_assets.empty()) {
      return;
    }
    new_info_file.open(assets_info_path.c_str(), std::ios::app);
  } else {
    new_info_file.open(assets_info_path.c_str());
    std::move(all_assets.begin(), all_assets.end(), std::back_inserter(new_assets));
  }
  for (const auto& asset : new_assets) {
    std::string new_entry;
    new_entry += std::to_string(++last_id);
    new_entry += ';';
    new_entry += asset;
    new_entry += ';';
    if (is_models_dir) {
      /// type & sounds_id should be added by user manually
      new_entry += ";;";
    }
    new_entry += '\n';
    new_info_file.write(new_entry.data(), new_entry.size());
  }
}

int main(int argc, char** argv) {
  static_assert(sizeof(float) == 4); // (need for hdr files) just in case ;)

  if (argc != 4) {
    std::cout << "Incorrect program's arguments!"
              << "\nfor encode: <destination> <source> e"
              << "\nfor decode: <destination> <source> d"
              << std::endl;
    return 1;
  }
  std::filesystem::path destination{argv[1]};
  std::filesystem::path source{argv[2]};
  bool encode;
  if (argv[3][0] == 'e') {
    encode = true;
  } else if (argv[3][0] == 'd') {
    encode = false;
  } else {
    std::cout << "Incorrect program's arguments!"
              << "\nfor encode: <destination> <source> e"
              << "\nfor decode: <destination> <source> d"
              << std::endl;
    return 2;
  }

  if (destination == source) {
    std::cerr << "source can't be equal to destination" << std::endl;
    return 3;
  }

  AssetProcessor processor_encoder(faithful::config::kMaxHardwareThread);
  try {
    processor_encoder.Process(destination, source, encode);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 4;
  }

  UpdateAssetsInfo(destination / "music");
  UpdateAssetsInfo(destination / "sounds");
  UpdateAssetsInfo(destination / "maps");
  UpdateAssetsInfo(destination / "noises");
  UpdateAssetsInfo(destination);

  /// models has slightly different file
  UpdateAssetsInfo(destination / "models", true);

  return 0;
}
