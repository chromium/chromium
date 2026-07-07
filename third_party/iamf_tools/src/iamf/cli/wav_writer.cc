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

#include "iamf/cli/wav_writer.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/types.h"
#include "src/dsp/write_wav_file.h"

namespace iamf_tools {

namespace {

using ::absl::MakeConstSpan;

// Some audio to tactile functions return 0 on success and 1 on failure.
constexpr int kAudioToTactileResultFailure = 0;

// This class is implemented to consume all samples without producing output
// samples.
constexpr size_t kMaxOutputSamplesPerFrame = 0;

// Write samples for all channels.
absl::Status WriteSamplesInternal(FILE* absl_nullable file, size_t num_channels,
                                  int bit_depth,
                                  size_t max_num_samples_per_frame,
                                  absl::Span<const uint8_t> buffer,
                                  size_t& total_samples_accumulator) {
  if (file == nullptr) {
    // Wav writer may have been aborted.
    return absl::FailedPreconditionError(
        "Wav writer is not accepting samples.");
  }

  const auto buffer_size = buffer.size();

  if (buffer_size == 0) {
    // Nothing to write.
    return absl::OkStatus();
  }

  if (buffer_size % (bit_depth * num_channels / 8) != 0) {
    return absl::InvalidArgumentError(
        "Must write an integer number of samples.");
  }

  // Calculate how many samples there are.
  const int bytes_per_sample = bit_depth / 8;
  const size_t num_total_samples = (buffer_size) / bytes_per_sample;
  const size_t num_samples_per_channel = num_total_samples / num_channels;
  if (num_samples_per_channel > max_num_samples_per_frame) {
    return absl::InvalidArgumentError(
        absl::StrCat("Too many samples per frame. The `WavWriter` is "
                     "configured with a maximum number of "
                     "samples per frame of: ",
                     max_num_samples_per_frame,
                     ". The number of samples per frame received is: ",
                     num_samples_per_channel));
  }

  const size_t elements_written =
      std::fwrite(buffer.data(), 1, buffer.size(), file);
  if (elements_written != buffer.size()) {
    return absl::ErrnoToStatus(
        errno,
        absl::StrCat("Error writing samples to wav file. elements_written= ",
                     elements_written, " buffer.size()=", buffer.size()));
  }

  total_samples_accumulator += num_total_samples;
  return absl::OkStatus();
}

void MaybeFinalizeFile(size_t sample_rate_hz, size_t num_channels,
                       auto& wav_header_writer, FILE*& file,
                       size_t& total_samples_written) {
  if (file == nullptr) {
    return;
  }

  // Finalize the temporary header based on the total number of samples written
  // and close the file.
  if (wav_header_writer) {
    std::fseek(file, 0, SEEK_SET);
    wav_header_writer(file, total_samples_written, sample_rate_hz,
                      num_channels);
  }
  std::fclose(file);
  file = nullptr;
}

}  // namespace

std::unique_ptr<WavWriter> WavWriter::Create(const std::string& wav_filename,
                                             int num_channels,
                                             int sample_rate_hz, int bit_depth,
                                             size_t num_samples_per_frame,
                                             bool write_header) {
  // Open the file to write to.
  ABSL_LOG(INFO) << "Writer \"" << wav_filename << "\"";
  auto* file = std::fopen(wav_filename.c_str(), "wb");
  if (file == nullptr) {
    ABSL_LOG(ERROR).WithPerror()
        << "Error opening file \"" << wav_filename << "\"";
    return nullptr;
  }

  // Write a dummy header. This will be overwritten in the destructor.
  WavHeaderWriter wav_header_writer;
  switch (bit_depth) {
    case 16:
      wav_header_writer = WriteWavHeader;
      break;
    case 24:
      wav_header_writer = WriteWavHeader24Bit;
      break;
    case 32:
      wav_header_writer = WriteWavHeader32Bit;
      break;
    default:
      ABSL_LOG(WARNING) << "This implementation does not support writing "
                        << bit_depth << "-bit wav files.";
      std::fclose(file);
      std::remove(wav_filename.c_str());
      return nullptr;
  }

  // Set to an empty writer. The emptiness can be checked to skip writing the
  // header.
  if (!write_header) {
    wav_header_writer = WavHeaderWriter();
  } else if (wav_header_writer(file, 0, sample_rate_hz, num_channels) ==
             kAudioToTactileResultFailure) {
    ABSL_LOG(ERROR) << "Error writing header of file \"" << wav_filename
                    << "\"";
    return nullptr;
  }

  return absl::WrapUnique(
      new WavWriter(wav_filename, num_channels, sample_rate_hz, bit_depth,
                    num_samples_per_frame, file, std::move(wav_header_writer)));
}

WavWriter::~WavWriter() {
  // Finalize the header, in case the user did not call `Flush()`.
  MaybeFinalizeFile(sample_rate_hz_, num_channels_, wav_header_writer_, file_,
                    total_samples_written_);
}

absl::Status WavWriter::PushFrameDerived(
    absl::Span<const absl::Span<const InternalSampleType>>
        channel_time_samples) {
  // Flatten down the serialized PCM for compatibility with the internal
  // `WriteSamplesInternal` function.
  const size_t num_channels = channel_time_samples.size();
  const size_t num_ticks =
      channel_time_samples.empty() ? 0 : channel_time_samples[0].size();
  if (!std::all_of(
          channel_time_samples.begin(), channel_time_samples.end(),
          [&](const auto& channel) { return channel.size() == num_ticks; })) {
    return absl::InvalidArgumentError(
        "All channels must have the same number of ticks.");
  }

  size_t write_position = 0;
  for (size_t t = 0; t < num_ticks; t++) {
    for (size_t c = 0; c < num_channels; c++) {
      int32_t sample_int32;
      RETURN_IF_NOT_OK(NormalizedFloatingPointToInt32(
          channel_time_samples[c][t], sample_int32));
      RETURN_IF_NOT_OK(WritePcmSample(
          static_cast<uint32_t>(sample_int32), bit_depth_,
          /*big_endian=*/false, reusable_pcm_buffer_.data(), write_position));
    }
  }
  // May have written less than a full frame, pass the valid portion.
  const absl::Span<const uint8_t> valid_pcm_samples =
      MakeConstSpan(reusable_pcm_buffer_).first(write_position);
  return WriteSamplesInternal(file_, num_channels_, bit_depth_,
                              max_input_samples_per_frame_, valid_pcm_samples,
                              total_samples_written_);
}

absl::Status WavWriter::FlushDerived() {
  // No more samples are coming, finalize the header and close the file.
  MaybeFinalizeFile(sample_rate_hz_, num_channels_, wav_header_writer_, file_,
                    total_samples_written_);
  return absl::OkStatus();
}

absl::Status WavWriter::WritePcmSamples(absl::Span<const uint8_t> buffer) {
  return WriteSamplesInternal(file_, num_channels_, bit_depth_,
                              max_input_samples_per_frame_, buffer,
                              total_samples_written_);
}

void WavWriter::Abort() {
  std::fclose(file_);
  std::remove(filename_to_remove_.c_str());
  file_ = nullptr;
}

WavWriter::WavWriter(const std::string& filename_to_remove, int num_channels,
                     int sample_rate_hz, int bit_depth,
                     size_t num_samples_per_frame, FILE* file,
                     WavHeaderWriter wav_header_writer)
    : SampleProcessorBase(num_samples_per_frame, num_channels,
                          kMaxOutputSamplesPerFrame),
      sample_rate_hz_(sample_rate_hz),
      bit_depth_(bit_depth),
      total_samples_written_(0),
      file_(file),
      filename_to_remove_(filename_to_remove),
      wav_header_writer_(std::move(wav_header_writer)),
      reusable_pcm_buffer_(
          num_channels * num_samples_per_frame * (bit_depth_ / 8), 0) {}

}  // namespace iamf_tools
