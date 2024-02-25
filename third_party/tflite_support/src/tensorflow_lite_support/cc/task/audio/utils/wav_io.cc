/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Functions to write audio in WAV format.
// This file is forked from `tensorflow/core/lib/wav/wav_io.cc`.

#include "tensorflow_lite_support/cc/task/audio/utils/wav_io.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <fstream>
#include <limits>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace task {
namespace audio {
namespace {

constexpr char kRiffChunkId[] = "RIFF";
constexpr char kRiffType[] = "WAVE";
constexpr char kFormatChunkId[] = "fmt ";
constexpr char kDataChunkId[] = "data";

inline float Int16SampleToFloat(int16_t data) {
  constexpr float kMultiplier = 1.0f / (1 << 15);
  return data * kMultiplier;
}

}  // namespace

std::string ReadFile(const std::string filepath) {
  std::ifstream fs(filepath);
  if (!fs.is_open()) {
    return "";
  }
  std::string contents((std::istreambuf_iterator<char>(fs)),
                       (std::istreambuf_iterator<char>()));
  return contents;
}

// Handles moving the data index forward, validating the arguments, and avoiding
// overflow or underflow.
absl::Status IncrementOffset(uint32_t old_offset, size_t increment,
                             size_t max_size, uint32_t* new_offset) {
  if (old_offset < 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Negative offsets are not allowed: %d", old_offset));
  }
  if (old_offset > max_size) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Initial offset is outside data range: %d", old_offset));
  }
  *new_offset = old_offset + increment;
  if (*new_offset > max_size) {
    return absl::InvalidArgumentError(
        "Data too short when trying to read string");
  }
  // See above for the check that the input offset is positive. If it's negative
  // here then it means that there's been an overflow in the arithmetic.
  if (*new_offset < 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Offset too large, overflowed: %d", *new_offset));
  }
  return absl::OkStatus();
}

absl::Status ExpectText(const std::string& data,
                        const std::string& expected_text, uint32_t* offset) {
  uint32_t new_offset;
  TFLITE_RETURN_IF_ERROR(
      IncrementOffset(*offset, expected_text.size(), data.size(), &new_offset));
  const std::string found_text(data.begin() + *offset,
                               data.begin() + new_offset);
  if (found_text != expected_text) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Header mismatch: Expected", expected_text, " but found ", found_text));
  }
  *offset = new_offset;
  return absl::OkStatus();
}

absl::Status ReadString(const std::string& data, size_t expected_length,
                        std::string* value, uint32_t* offset) {
  uint32_t new_offset;
  TFLITE_RETURN_IF_ERROR(
      IncrementOffset(*offset, expected_length, data.size(), &new_offset));
  *value = std::string(data.begin() + *offset, data.begin() + new_offset);
  *offset = new_offset;
  return absl::OkStatus();
}

absl::Status DecodeLin16WaveAsFloatVector(const std::string& wav_string,
                                          std::vector<float>* float_values,
                                          uint32_t* offset,
                                          uint32_t* sample_count,
                                          uint16_t* channel_count,
                                          uint32_t* sample_rate) {
  TFLITE_RETURN_IF_ERROR(ExpectText(wav_string, kRiffChunkId, offset));
  uint32_t total_file_size;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_string, &total_file_size, offset));
  TFLITE_RETURN_IF_ERROR(ExpectText(wav_string, kRiffType, offset));
  TFLITE_RETURN_IF_ERROR(ExpectText(wav_string, kFormatChunkId, offset));
  uint32_t format_chunk_size;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_string, &format_chunk_size, offset));
  if ((format_chunk_size != 16) && (format_chunk_size != 18)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Bad format chunk size for WAV: Expected 16 or 18, but got %" PRIu32,
        format_chunk_size));
  }
  uint16_t audio_format;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_string, &audio_format, offset));
  if (audio_format != 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Bad audio format for WAV: Expected 1 (PCM), but got %" PRIu16,
        audio_format));
  }
  TFLITE_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_string, channel_count, offset));
  if (*channel_count < 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Bad number of channels for WAV: Expected at least 1, but got %" PRIu16,
        *channel_count));
  }
  TFLITE_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_string, sample_rate, offset));
  uint32_t bytes_per_second;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_string, &bytes_per_second, offset));
  uint16_t bytes_per_sample;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_string, &bytes_per_sample, offset));
  // Confusingly, bits per sample is defined as holding the number of bits for
  // one channel, unlike the definition of sample used elsewhere in the WAV
  // spec. For example, bytes per sample is the memory needed for all channels
  // for one point in time.
  uint16_t bits_per_sample;
  TFLITE_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_string, &bits_per_sample, offset));
  if (bits_per_sample != 16) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Can only read 16-bit WAV files, but received %" PRIu16,
                        bits_per_sample));
  }
  const uint32_t expected_bytes_per_sample =
      ((bits_per_sample * *channel_count) + 7) / 8;
  if (bytes_per_sample != expected_bytes_per_sample) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Bad bytes per sample in WAV header: Expected %" PRIu32
                        " but got %" PRIu16,
                        expected_bytes_per_sample, bytes_per_sample));
  }
  const uint32_t expected_bytes_per_second = bytes_per_sample * *sample_rate;
  if (bytes_per_second != expected_bytes_per_second) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Bad bytes per second in WAV header: Expected %" PRIu32
                        " but got %" PRIu32 " (sample_rate=%" PRIu32
                        ", bytes_per_sample=%" PRIu16 ")",
                        expected_bytes_per_second, bytes_per_second,
                        *sample_rate, bytes_per_sample));
  }
  if (format_chunk_size == 18) {
    // Skip over this unused section.
    *offset += 2;
  }

  bool was_data_found = false;
  while (*offset < wav_string.size()) {
    std::string chunk_id;
    TFLITE_RETURN_IF_ERROR(ReadString(wav_string, 4, &chunk_id, offset));
    uint32_t chunk_size;
    TFLITE_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_string, &chunk_size, offset));
    if (chunk_size > std::numeric_limits<int32_t>::max()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "WAV data chunk '%s' is too large: %" PRIu32
          " bytes, but the limit is %d",
          chunk_id.c_str(), chunk_size, std::numeric_limits<int32_t>::max()));
    }
    if (chunk_id == kDataChunkId) {
      if (was_data_found) {
        return absl::InvalidArgumentError(
            "More than one data chunk found in WAV");
      }
      was_data_found = true;
      *sample_count = chunk_size / bytes_per_sample;
      const uint32_t data_count = *sample_count * *channel_count;
      uint32_t unused_new_offset = 0;
      // Validate that the data exists before allocating space for it
      // (prevent easy OOM errors).
      TFLITE_RETURN_IF_ERROR(IncrementOffset(*offset, sizeof(int16_t) * data_count,
                                      wav_string.size(), &unused_new_offset));
      float_values->resize(data_count);
      for (int i = 0; i < data_count; ++i) {
        int16_t single_channel_value = 0;
        TFLITE_RETURN_IF_ERROR(
            ReadValue<int16_t>(wav_string, &single_channel_value, offset));
        (*float_values)[i] = Int16SampleToFloat(single_channel_value);
      }
    } else {
      *offset += chunk_size;
    }
  }
  if (!was_data_found) {
    return absl::InvalidArgumentError("No data chunk found in WAV");
  }
  return absl::OkStatus();
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
