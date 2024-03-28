#include "AudioProcessor.h"

AudioProcessor::AudioProcessor(
    ReplaceRequest& replace_request)
    : replace_request_(replace_request) {}


void AudioProcessor::EncodeMusic(const std::filesystem::path& path) {
  std::string out_filename =
      (music_destination_path_ / path.filename()).string();
  if (std::filesystem::exists(out_filename)) {
    std::string request{out_filename};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return;
    }
  }
  std::filesystem::copy_file(
      path, out_filename, std::filesystem::copy_options::update_existing);
}

void AudioProcessor::EncodeSound(const std::filesystem::path& path) {
  std::string out_filename =
      (sounds_destination_path_ / path.filename()).string();
  if (std::filesystem::exists(out_filename)) {
    std::string request{out_filename};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return;
    }
  }
  std::filesystem::copy_file(
      path, out_filename, std::filesystem::copy_options::update_existing);
}

void AudioProcessor::DecodeMusic(const std::filesystem::path& path) {
  EncodeMusic(path);
}

void AudioProcessor::DecodeSound(const std::filesystem::path& path) {
  EncodeSound(path);
}

void AudioProcessor::SetDestinationDirectory(
    const std::filesystem::path& path) {
  sounds_destination_path_ = path / "sounds";
  music_destination_path_ = path / "music";
  std::filesystem::create_directories(sounds_destination_path_);
  std::filesystem::create_directories(music_destination_path_);
}
