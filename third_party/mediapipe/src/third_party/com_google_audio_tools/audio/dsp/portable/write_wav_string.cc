/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/portable/write_wav_string.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "audio/dsp/portable/write_wav_file_generic.h"

namespace audio_dsp {

using ::std::vector;

static size_t WriteBytesToString(const void* bytes, size_t num_bytes,
                                 void* io_ptr) {
  reinterpret_cast<std::string*>(io_ptr)->append(
      reinterpret_cast<const char*>(bytes), num_bytes);
  return num_bytes;
}

static WavWriter WavWriterInMemoryString(std::string* s) {
  WavWriter w;
  w.write_fun = WriteBytesToString;
  w.io_ptr = s;
  return w;
}

int WriteWavToString(const int16_t* samples,
                     size_t num_samples,
                     int sample_rate_hz,
                     int num_channels,
                     std::string* string_data) {
  string_data->clear();
  WavWriter w = WavWriterInMemoryString(string_data);
  if (!WriteWavHeaderGeneric(&w, num_samples, sample_rate_hz, num_channels)) {
    return false;
  }
  return WriteWavSamplesGeneric(&w, samples, num_samples);
}

void FloatSamplesToInt(const float* input, int num_elements,
                       size_t input_offset, vector<int16_t>* output) {
  constexpr float kLowest = std::numeric_limits<int16_t>::lowest();
  constexpr float kMax = std::numeric_limits<int16_t>::max();
  constexpr float normalizer = -kLowest;
  for (int i = 0; i < num_elements; ++i) {
    float f =
        std::max(kLowest, std::min(input[i + input_offset] * normalizer, kMax));
    if (std::isnan(f)) { f = 0; }
    (*output)[i] = static_cast<int16_t>(f);
  }
}

int WriteWavToString(const float* samples,
                     size_t num_samples,
                     int sample_rate_hz,
                     int num_channels,
                     std::string* string_data) {
  string_data->clear();
  WavWriter w = WavWriterInMemoryString(string_data);
  if (!WriteWavHeaderGeneric(&w, num_samples, sample_rate_hz, num_channels)) {
    return false;
  }
  size_t position = 0;
  // Do buffered reading so we don't have two copies of the full signal
  // in memory.
  constexpr int kBufferSize = 512;
  vector<int16_t> buffer(kBufferSize);
  while (num_samples - position > kBufferSize) {
    FloatSamplesToInt(samples, kBufferSize, position, &buffer);
    WriteWavSamplesGeneric(&w, buffer.data(), kBufferSize);
    position += kBufferSize;
  }
  if (num_samples != position) {
    FloatSamplesToInt(samples, num_samples - position, position, &buffer);
    WriteWavSamplesGeneric(&w, buffer.data(), num_samples - position);
  }
  return 1;
}

}  // namespace audio_dsp
