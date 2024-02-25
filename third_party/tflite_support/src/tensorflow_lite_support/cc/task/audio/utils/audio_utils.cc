/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/task/audio/utils/audio_utils.h"

namespace tflite {
namespace task {
namespace audio {

tflite::support::StatusOr<AudioBuffer> LoadAudioBufferFromFile(
    const std::string& wav_file_path, uint32_t* buffer_size, uint32_t* offset,
    std::vector<float>* wav_data) {
  std::string contents = ReadFile(wav_file_path);

  uint32_t decoded_sample_count;
  uint16_t decoded_channel_count;
  uint32_t decoded_sample_rate;

  TFLITE_RETURN_IF_ERROR(DecodeLin16WaveAsFloatVector(
      contents, wav_data, offset, &decoded_sample_count, &decoded_channel_count,
      &decoded_sample_rate));

  if (decoded_sample_count > *buffer_size) {
    decoded_sample_count = *buffer_size;
  }

  return AudioBuffer(
      wav_data->data(), static_cast<int>(decoded_sample_count),
      {decoded_channel_count, static_cast<int>(decoded_sample_rate)});
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
