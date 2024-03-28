#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H
#define FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H

#include <filesystem>

#include "AssetLoadingThreadPool.h"
#include "ReplaceRequest.h"

/// CURRENTLY just copy of .ogg & .wav into destination

// TODO(dr_libs): mp3/flac/ogg/wav -> ogg/wav (depends on size)

class AudioProcessor {
 public:
  AudioProcessor() = delete;
  AudioProcessor(ReplaceRequest& replace_request);

  /// non-assignable because of member reference
  AudioProcessor(const AudioProcessor&) = delete;
  AudioProcessor& operator=(const AudioProcessor&) = delete;

  AudioProcessor(AudioProcessor&&) = default;
  AudioProcessor& operator=(AudioProcessor&&) = delete;

  void EncodeMusic(const std::filesystem::path& path);
  void EncodeSound(const std::filesystem::path& path);
  void DecodeMusic(const std::filesystem::path& path);
  void DecodeSound(const std::filesystem::path& path);

  void SetDestinationDirectory(const std::filesystem::path& path);

 private:
  ReplaceRequest& replace_request_;

  std::filesystem::path sounds_destination_path_;
  std::filesystem::path music_destination_path_;
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H
