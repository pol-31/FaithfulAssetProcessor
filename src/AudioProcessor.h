#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H
#define FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H

#include <filesystem>
#include <fstream>
#include <vector>

#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"
#include "vorbis/codec.h"

#include "dr_mp3.h"
#include "dr_flac.h"
#include "dr_wav.h"

#include "AssetLoadingThreadPool.h"
#include "ReplaceRequest.h"

class AudioProcessor {
 public:struct ThreadData {
    vorbis_dsp_state vd;
    ogg_stream_state os;
    vorbis_block vb;
    ogg_packet op;
    ogg_page og;
    vorbis_info vi;
    vorbis_comment vc;
    std::unique_ptr<uint8_t[]> buffer;
    int buffer_data_length;

    void Init(int channels, int sample_rate);
    void DeInit();
  };

  AudioProcessor() = delete;
  AudioProcessor(AssetLoadingThreadPool& thread_pool,
                 ReplaceRequest& replace_request);

  /// non-assignable because of member reference
  AudioProcessor(const AudioProcessor&) = delete;
  AudioProcessor& operator=(const AudioProcessor&) = delete;

  AudioProcessor(AudioProcessor&&) = default;
  AudioProcessor& operator=(AudioProcessor&&) = delete;

  void Encode(const std::filesystem::path& path);
  void Decode(const std::filesystem::path& path);

  void SetDestinationDirectory(const std::filesystem::path& path);

 private:
  enum class AudioFormat {
    kFlac,
    kMp3,
    kOgg,
    kWav
  };
  enum class AudioSize {
    kMusic,
    kSound
  };

  void EncodeMusic(const std::filesystem::path& path, AudioFormat format);
  void EncodeSound(const std::filesystem::path& path, AudioFormat format);

  static uint64_t DecompressFlacChunk(drflac& drflac_context, float* pcm_data,
                                      std::size_t buffer_size, uint64_t* frames);
  static uint64_t DecompressMp3Chunk(drmp3& drmp3_context, float* pcm_data,
                                     std::size_t buffer_size, uint64_t* frames);
  static uint64_t DecompressWavChunk(drwav& drwav_context, float* pcm_data,
                                     std::size_t buffer_size, uint64_t* frames);

  void CompressChunk(const float* pcm_data, uint64_t frames, int channels);

  void WriteCompressedOggChunks();

  void PrepareEncodingContext(int channels, int sample_rate);
  void UpdateThreadsBuffers();

  std::filesystem::path asset_destination_;

  std::vector<AudioProcessor::ThreadData> threads_info_;

  AssetLoadingThreadPool& thread_pool_;
  int thread_number_;
  ReplaceRequest& replace_request_;

  std::filesystem::path out_filename_;

  std::filesystem::path sounds_destination_path_;
  std::filesystem::path music_destination_path_;
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_AUDIOPROCESSOR_H
