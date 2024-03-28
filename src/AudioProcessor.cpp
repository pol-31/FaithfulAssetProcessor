#include "AudioProcessor.h"

#include <iostream>

#include "../config/AssetFormats.h"

/* 1 thread:
 * - slow (1 min - 2s, what is too much and any online converted is faster)
 * N threads:
 * - artifacts between threads data concatenation (looks like I don't
 *   understand how libvorbis works OR I need to implemented a MDCT, what is
 *   also seems weird, because libvorbis probably should have been implemented
 *   at least "small support" for parallel processing)
 * - incorrect packet order (yes, ogg spec allows just to concatenate audio and
 *   in some ogg-file players it's supported, but not in all, what makes it
 *   inconvenient)
 * */

void AudioProcessor::ThreadData::Init(int channels, int sample_rate) {
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  vorbis_encode_init_vbr(&vi, channels, sample_rate,
                         faithful::config::kAudioCompQuality);

  vorbis_analysis_init(&vd, &vi);
  vorbis_block_init(&vd, &vb);

  // TODO: don't need srand(time) because we simply concatenating audio
  ogg_stream_init(&os, 0);

  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;

  vorbis_analysis_headerout(&vd, &vc, &header,
                            &header_comm, &header_code);
  ogg_stream_packetin(&os, &header);
  ogg_stream_packetin(&os, &header_comm);
  ogg_stream_packetin(&os, &header_code);
}

void AudioProcessor::ThreadData::DeInit() {
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
}

AudioProcessor::AudioProcessor(
    AssetLoadingThreadPool& thread_pool,
    ReplaceRequest& replace_request)
    : thread_pool_(thread_pool),
      thread_number_(thread_pool_.GetThreadNumber()),
      replace_request_(replace_request) {}


void AudioProcessor::Encode(const std::filesystem::path& path) {
  AudioSize audio_size = AudioSize::kMusic;
  AudioFormat audio_format;
  if (path.extension() == ".flac") {
    audio_format = AudioFormat::kFlac;
    if (file_size(path) < faithful::config::kMusicFlacThreshold) {
      audio_size = AudioSize::kSound;
    }
  } else if (path.extension() == ".ogg") {
    audio_format = AudioFormat::kOgg;
    if (file_size(path) < faithful::config::kMusicOggThreshold) {
      audio_size = AudioSize::kSound;
    }
  } else if (path.extension() == ".mp3") {
    audio_format = AudioFormat::kMp3;
    if (file_size(path) < faithful::config::kMusicMp3Threshold) {
      audio_size = AudioSize::kSound;
    }
  } else if (path.extension() == ".wav") {
    audio_format = AudioFormat::kWav;
    if (file_size(path) < faithful::config::kMusicWavThreshold) {
      audio_size = AudioSize::kSound;
    }
  } else {
    throw;
  }
  std::filesystem::path out_filename =
      sounds_destination_path_ / path.filename();
  if (audio_size == AudioSize::kMusic) {
    out_filename.replace_extension(".ogg");
  } else {
    out_filename.replace_extension(".wav");
  }
  if (std::filesystem::exists(out_filename)) {
    std::string request{out_filename};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return;
    }
  }
  out_filename_ = out_filename;
  if (audio_size == AudioSize::kMusic) {
    EncodeMusic(path, audio_format);
  } else {
    EncodeSound(path, audio_format);
  }
}

void AudioProcessor::Decode(const std::filesystem::path& path) {
  /// audio already in listenable format, so just copy
  std::filesystem::path out_filename;
  if (path.extension() == ".ogg") {
    out_filename = sounds_destination_path_ / path.filename();
  } else if (path.extension() == ".wav") {
    out_filename = music_destination_path_ / path.filename();
  } else {
    throw;
  }
  if (std::filesystem::exists(out_filename)) {
    std::string request{out_filename};
    request += "\nalready exist. Do you want to replace it?";
    if (!replace_request_(std::move(request))) {
      return;
    }
  }
  std::filesystem::copy_file(path, out_filename);
}

void AudioProcessor::EncodeMusic(const std::filesystem::path& path,
                                 AudioFormat format) {
  size_t buffer_size = faithful::config::kAudioTotalChunkBufferSize;
  auto pcm_data = std::make_unique<float[]>(buffer_size);
  uint64_t frames;

  if (format == AudioFormat::kFlac) {
    drflac* flac_context = drflac_open_file(path.string().c_str(), nullptr);
    if (!flac_context) {
      std::cerr << "Error: cannot open flac file: " << path << std::endl;
      return;
    }
    PrepareEncodingContext(static_cast<int>(flac_context->channels),
                           static_cast<int>(flac_context->sampleRate));
    while (true) {
      UpdateThreadsBuffers();
      DecompressFlacChunk(*flac_context, pcm_data.get(), buffer_size, &frames);
      if (frames == 0) {
        break;
      }
      CompressChunk(pcm_data.get(), frames, flac_context->channels);
    }
    drflac_close(flac_context);
  } else if (format == AudioFormat::kMp3) {
    drmp3 drmp3_context;
    if (!drmp3_init_file(&drmp3_context, path.string().c_str(), nullptr)) {
      std::cerr << "Error: cannot open mp3 file: " << path << std::endl;
      return;
    }
    PrepareEncodingContext(static_cast<int>(drmp3_context.channels),
                           static_cast<int>(drmp3_context.sampleRate));
    while (true) {
      UpdateThreadsBuffers();
      DecompressMp3Chunk(drmp3_context, pcm_data.get(), buffer_size, &frames);
      if (frames == 0) {
        break;
      }
      CompressChunk(pcm_data.get(), frames,
                    static_cast<int>(drmp3_context.sampleRate));
    }
    drmp3_uninit(&drmp3_context);
  } else if (format == AudioFormat::kOgg) {
    std::filesystem::copy_file(path, out_filename_);
  } else if (format == AudioFormat::kWav) {
    drwav drwav_context;
    if (!drwav_init_file(&drwav_context, path.string().c_str(), nullptr)) {
      std::cerr << "Error: cannot open wav file: " << path << std::endl;
      return;
    }
    PrepareEncodingContext(static_cast<int>(drwav_context.channels),
                           static_cast<int>(drwav_context.sampleRate));
    while (true) {
      UpdateThreadsBuffers();
      DecompressWavChunk(drwav_context, pcm_data.get(), buffer_size, &frames);
      if (frames == 0) {
        break;
      }
      CompressChunk(pcm_data.get(), frames, drwav_context.channels);
    }
    drwav_uninit(&drwav_context);
  } else {
    throw;
  }
}

/*TODO:
 * - DecompressOggChunk() <- vorbisfile?
 * - CompressWavChunk()
 * */

void AudioProcessor::PrepareEncodingContext(int channels, int sample_rate) {
  for (std::size_t i = 0; i < threads_info_.size(); ++i) {
    threads_info_[i].DeInit();
    threads_info_[i].Init(channels, sample_rate);
    if (i != 0) {
      while (ogg_stream_flush(&threads_info_[i].os, &threads_info_[i].og)) {
      }
    }
  }
  std::ofstream out_file(out_filename_, std::ios::binary);
  while (ogg_stream_flush(&threads_info_[0].os, &threads_info_[0].og)) {
    out_file.write(reinterpret_cast<const char*>(threads_info_[0].og.header),
                   threads_info_[0].og.header_len);
    out_file.write(reinterpret_cast<const char*>(threads_info_[0].og.body),
                   threads_info_[0].og.body_len);
  }
}

void AudioProcessor::UpdateThreadsBuffers() {
  for (auto& thread_data : threads_info_) {
    thread_data.buffer = std::make_unique<uint8_t[]>(
        faithful::config::kAudioThreadChunkBufferSize);
    thread_data.buffer_data_length = 0;
  }
}

void AudioProcessor::EncodeSound(const std::filesystem::path& path,
                                 AudioFormat format) {
  size_t buffer_size = faithful::config::kAudioTotalChunkBufferSize;
  auto pcm_data = std::make_unique<float[]>(buffer_size);
  uint64_t frames;

  /// ONLY ONE ITERATION (comparing to EncodeMusic where when have while(true)
  UpdateThreadsBuffers();
  if (format == AudioFormat::kFlac) {
    drflac* flac_context = drflac_open_file(path.string().c_str(), nullptr);
    if (!flac_context) {
      std::cerr << "Error: cannot open flac file: " << path << std::endl;
      return;
    }
    PrepareEncodingContext(static_cast<int>(flac_context->channels),
                           static_cast<int>(flac_context->sampleRate));
    if (DecompressFlacChunk(*flac_context, pcm_data.get(),
                            buffer_size, &frames)) {
      CompressChunk(pcm_data.get(), frames, flac_context->channels);
    }
    drflac_close(flac_context);
  } else if (format == AudioFormat::kMp3) {
    drmp3 drmp3_context;
    if (!drmp3_init_file(&drmp3_context, path.string().c_str(), nullptr)) {
      std::cerr << "Error: cannot open mp3 file: " << path << std::endl;
      return;
    }
    PrepareEncodingContext(static_cast<int>(drmp3_context.channels),
                           static_cast<int>(drmp3_context.sampleRate));
    if (DecompressMp3Chunk(drmp3_context, pcm_data.get(), buffer_size, &frames)) {
      CompressChunk(pcm_data.get(), frames,
                    static_cast<int>(drmp3_context.channels));
    }
    drmp3_uninit(&drmp3_context);
  } else if (format == AudioFormat::kOgg) {
    std::terminate(); // NOT IMPLEMENTED
  } else if (format == AudioFormat::kWav) {
    std::filesystem::copy_file(path, out_filename_);
  } else {
    throw;
  }
}

uint64_t AudioProcessor::DecompressFlacChunk(
    drflac& drflac_context, float* pcm_data,
    std::size_t buffer_size, uint64_t* frames) {
  int channels = drflac_context.channels;
  *frames = 0;
  size_t framesRead;
  while ((framesRead = drflac_read_pcm_frames_f32(
      &drflac_context, faithful::config::kAudioDecompChunkSize,
      pcm_data + (*frames * channels))) > 0) {
    *frames += framesRead;
    if (buffer_size < (*frames + 2 * faithful::config::kAudioDecompChunkSize)
        * channels)
      break;
  }
  return *frames;
}

uint64_t AudioProcessor::DecompressMp3Chunk(
    drmp3& drmp3_context, float* pcm_data,
    std::size_t buffer_size, uint64_t* frames) {
  auto channels = static_cast<int>(drmp3_context.channels);
  *frames = 0;
  size_t framesRead;
  while ((framesRead = drmp3_read_pcm_frames_f32(
      &drmp3_context, faithful::config::kAudioDecompChunkSize,
      pcm_data + (*frames * channels))) > 0) {
    *frames += framesRead;
    if (buffer_size < (*frames
                       + faithful::config::kAudioDecompChunkSize) * channels) {
      break;
    }
  }
  std::cerr
      << buffer_size << " "
      << *frames << " "
      << faithful::config::kAudioDecompChunkSize << " "
      << channels << " "
      << ((*frames + faithful::config::kAudioDecompChunkSize) * channels)
      << std::endl;
  return *frames;
}

uint64_t AudioProcessor::DecompressWavChunk(
    drwav& drwav_context, float* pcm_data,
    std::size_t buffer_size, uint64_t* frames) {
  int channels = drwav_context.channels;
  *frames = 0;
  size_t framesRead;
  while ((framesRead = drwav_read_pcm_frames_f32(
      &drwav_context, faithful::config::kAudioDecompChunkSize,
      pcm_data + (*frames * channels))) > 0) {
    *frames += framesRead;
    if (buffer_size < (*frames + 2 * faithful::config::kAudioDecompChunkSize)
        * channels) {
      break;
    }
  }
  return *frames;
}

void AudioProcessor::CompressChunk(
    const float* pcm_data, uint64_t frames, int channels) {
  int thread_offset = static_cast<int>(frames / thread_number_);

  folly::Function<void(int)> task = {
      [&, chunk_size = faithful::config::kAudioCompChunkSize,
          thread_offset](int thread_id) {
        if (thread_id != 0) {
          return;
        }
        ThreadData& context = threads_info_[thread_id];
        int cur_pos, next_pos;
        int sentinel;

        /// if current piece small enough to process with single thread
        if (thread_id == -1) {
          std::terminate(); // NOT IMPLEMENTED
        }

        next_pos = thread_id * thread_offset * channels;
        float** buffer = vorbis_analysis_buffer(&context.vd, chunk_size);

        /////////------------___overlap & add___-------------////////////
        /// --- WRONG ---
        /*if (thread_id != 0) {
          cur_pos = thread_id * thread_offset * channels - chunk_size * channels;
          for (int j = 0; j < channels; ++j) {
            for (long i = 0; i < faithful::config::kAudioCompChunkSize; ++i) {
              buffer[j][i] = pcm_data[cur_pos + i * channels + j];
            }
          }
          vorbis_analysis_wrote(&context.vd, faithful::config::kAudioCompChunkSize);
          vorbis_analysis_blockout()
          while (vorbis_analysis_blockout(&context.vd, &context.vb) == 1) {
            vorbis_analysis(&context.vb, nullptr);
            vorbis_bitrate_addblock(&context.vb);
            vorbis_bitrate_flushpacket(&context.vd, &context.op);
            ogg_stream_packetin(&context.os, &context.op);
            ogg_stream_flush(&context.os, &context.og);
            while (ogg_stream_pageout(&context.os, &context.og)) {
            }
          }
        }*/

        std::ofstream out_file(out_filename_, std::ios::binary | std::ios::app);
        while (true) {
          cur_pos = next_pos;
          next_pos += chunk_size * channels;

          if (thread_id != thread_number_ - 1) {
            sentinel = (thread_id + 1) * thread_offset * channels - next_pos;
          } else {
            sentinel = static_cast<int>(frames * channels - next_pos);
          }

          if (sentinel >= 0) {
            sentinel = chunk_size;
          } else {
//            next_pos = (thread_id + 1) * thread_offset * channels;
            sentinel = chunk_size + sentinel / channels;
          }

          for (int j = 0; j < channels; ++j) {
            for (long i = 0; i < sentinel; ++i) {
              buffer[j][i] = pcm_data[cur_pos + i * channels + j];
            }
          }
          vorbis_analysis_wrote(&context.vd, sentinel);

          while (vorbis_analysis_blockout(&context.vd, &context.vb) == 1) {
            vorbis_analysis(&context.vb, nullptr);
            vorbis_bitrate_addblock(&context.vb);
            while (vorbis_bitrate_flushpacket(&context.vd, &context.op)) {
              ogg_stream_packetin(&context.os, &context.op);
              while (ogg_stream_pageout(&context.os, &context.og)) {
                std::memmove(
                    threads_info_[thread_id].buffer.get()
                    + threads_info_[thread_id].buffer_data_length,
                    reinterpret_cast<const void*>(context.og.header),
                    context.og.header_len);
                threads_info_[thread_id].buffer_data_length
                += static_cast<int>(context.og.header_len);
                std::memmove(
                    threads_info_[thread_id].buffer.get()
                    + threads_info_[thread_id].buffer_data_length,
                    reinterpret_cast<const void*>(context.og.body),
                    context.og.body_len);
                threads_info_[thread_id].buffer_data_length
                += static_cast<int>(context.og.body_len);
              }
            }
          }
          if (sentinel != chunk_size) {
            break;
          }
        }

        // TODO: last flush?
        /*ogg_stream_flush(&context.os, &context.og);
        ogg_stream_pageout(&context.os, &context.og);
        std::memmove(thread_info_[thread_id].buffer.get() + thread_info_[thread_id].buffer_data_length,
                     reinterpret_cast<const void*>(context.og.header),
                     context.og.header_len);
        thread_info_[thread_id].buffer_data_length += context.og.header_len;
        std::memmove(thread_info_[thread_id].buffer.get() + thread_info_[thread_id].buffer_data_length,
                     reinterpret_cast<const void*>(context.og.body),
                     context.og.body_len);
        thread_info_[thread_id].buffer_data_length += context.og.body_len;*/
      }
  };

  if (frames < faithful::config::kAudioCompThreshold) {
    task(-1);
  } else {
    thread_pool_.Execute(std::move(task));
  }

  WriteCompressedOggChunks();
}

void AudioProcessor::WriteCompressedOggChunks() {
  std::ofstream out_file(out_filename_, std::ios::binary | std::ios::app);
  for (auto& thread_data : threads_info_) {
//    out_file.write(thread_data.overlapped_left.data(), overlapped_left.size());
    out_file.write(reinterpret_cast<const char*>(thread_data.buffer.get()), thread_data.buffer_data_length);
    std::cerr << "write + " << thread_data.buffer_data_length << std::endl;
//    out_file.write(thread_data.overlapped_right.data(), overlapped_right.size());
  }
}

void AudioProcessor::SetDestinationDirectory(
    const std::filesystem::path& path) {
  sounds_destination_path_ = path / "sounds";
  music_destination_path_ = path / "music";
  std::filesystem::create_directories(sounds_destination_path_);
  std::filesystem::create_directories(music_destination_path_);
}
