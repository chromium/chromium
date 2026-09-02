/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/wav_reader.h"

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/dsp/read_wav_file.h"
#include "src/dsp/read_wav_info.h"

namespace iamf_tools {

namespace {
const int kAudioToTactileFailure = 0;

// Converts one sample of an IEEE float WAV to the left-justified int32
// convention documented on `WavReader::buffers_`. For float sources
// `ReadWavSamples` fills the 32-bit destination with raw float values
// (`ReadWavInfo::sample_format == kFloat`), so `sample_bits` holds a float bit
// pattern, not an integer sample. The scaling matches audio_to_tactile's
// `InPlaceFloatToInt32Conversion` (which only runs on the whole-file
// `ReadWavFile` path): NaN maps to 0 and out-of-range values are clamped.
int32_t NormalizedFloatBitsToInt32(int32_t sample_bits) {
  static_assert(sizeof(float) == sizeof(int32_t));
  float sample_f32;
  std::memcpy(&sample_f32, &sample_bits, sizeof(sample_f32));
  if (std::isnan(sample_f32)) {
    return 0;
  }
  sample_f32 *= 2147483648.0f;  // Scale [-1, 1] to the int32 range.
  // Beware that a 32-bit float cannot represent INT32_MAX exactly;
  // (float)INT32_MAX == 2147483648.0f, which would overflow if cast.
  if (sample_f32 >= 2147483648.0f) {
    return std::numeric_limits<int32_t>::max();
  }
  if (sample_f32 <= -2147483648.0f) {
    return std::numeric_limits<int32_t>::min();
  }
  return static_cast<int32_t>(sample_f32);
}

}  // namespace

absl::StatusOr<WavReader> WavReader::CreateFromFile(
    const std::string& wav_filename, const size_t num_samples_per_frame) {
  if (num_samples_per_frame == 0) {
    return absl::InvalidArgumentError("num_samples_per_frame must be > 0");
  }
  ABSL_LOG(INFO) << "Reading \"" << wav_filename << "\"";
  FILE* file = std::fopen(wav_filename.c_str(), "rb");
  if (file == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to open file: \"", wav_filename,
                     "\" with error: ", std::strerror(errno), "."));
  }

  ReadWavInfo info;
  if (ReadWavHeader(file, &info) == kAudioToTactileFailure) {
    // Ownership of `file` only transfers to the `WavReader` on the success
    // path below, so it must be closed here to avoid leaking the descriptor
    // on every malformed input.
    std::fclose(file);
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to read header of file: \"", wav_filename,
                     "\". Maybe it is not a valid RIFF WAV."));
  }

  // Overwrite `info_.destination_alignment_bytes` to 4 to always store results
  // in 4 bytes (32 bits), so we can handle 16-, 24-, and 32-bit PCMs.
  info.destination_alignment_bytes = 4;

  // Log the header info.
  ABSL_LOG(INFO) << "WAV header info:";
  ABSL_LOG(INFO) << "  num_channels= " << info.num_channels;
  ABSL_LOG(INFO) << "  sample_rate_hz= " << info.sample_rate_hz;
  ABSL_LOG(INFO) << "  remaining_samples= " << info.remaining_samples;
  ABSL_LOG(INFO) << "  bit_depth= " << info.bit_depth;
  ABSL_LOG(INFO) << "  destination_alignment_bytes= "
                 << info.destination_alignment_bytes;
  ABSL_LOG(INFO) << "  encoding= " << info.encoding;
  ABSL_LOG(INFO) << "  sample_format= " << info.sample_format;

  return WavReader(num_samples_per_frame, file, info);
}

WavReader::WavReader(const size_t num_samples_per_frame, FILE* file,
                     const ReadWavInfo& info)
    : buffers_(info.num_channels, std::vector<int32_t>(num_samples_per_frame)),
      num_samples_per_frame_(num_samples_per_frame),
      file_(file),
      info_(info) {}

WavReader::WavReader(WavReader&& original)
    : buffers_(std::move(original.buffers_)),
      num_samples_per_frame_(original.num_samples_per_frame_),
      file_(original.file_),
      info_(original.info_) {
  // Invalidate the file pointer on the original copy to prevent it from being
  // closed on destruction.
  original.file_ = nullptr;
}

WavReader::~WavReader() {
  if (file_ != nullptr) {
    std::fclose(file_);
  }
}

size_t WavReader::ReadFrame() {
  // Read samples in an interleaved mannar but store the outputs in
  // (channel, time) axes.
  size_t samples_read = 0;
  const auto num_channels = info_.num_channels;
  std::vector<int32_t> buffer_of_one_tick(num_channels, 0);
  for (size_t t = 0; t < num_samples_per_frame_; t++) {
    samples_read +=
        ReadWavSamples(file_, &info_, buffer_of_one_tick.data(), num_channels);
    if (samples_read < static_cast<size_t>(num_channels)) {
      break;
    }
    for (int c = 0; c < num_channels; c++) {
      buffers_[c][t] = info_.sample_format == ReadWavInfo::kFloat
                           ? NormalizedFloatBitsToInt32(buffer_of_one_tick[c])
                           : buffer_of_one_tick[c];
    }
  }

  return samples_read;
}

}  // namespace iamf_tools
