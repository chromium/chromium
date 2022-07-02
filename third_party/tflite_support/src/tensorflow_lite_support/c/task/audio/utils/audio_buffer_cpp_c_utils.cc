/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/c/task/audio/utils/audio_buffer_cpp_c_utils.h"

#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"

namespace tflite {
namespace task {
namespace audio {

namespace {
using AudioBufferCpp = ::tflite::task::audio::AudioBuffer;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
}  // namespace

StatusOr<std::unique_ptr<AudioBufferCpp>> CreateCppAudioBuffer(
    const TfLiteAudioBuffer* audio_buffer) {
  if (audio_buffer == nullptr)
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null audio buffer."),
        TfLiteSupportStatus::kInvalidArgumentError);

  return AudioBufferCpp::Create(
      audio_buffer->data, audio_buffer->size,
      {audio_buffer->format.channels,
       static_cast<int>(audio_buffer->format.sample_rate)});
}

StatusOr<TfLiteAudioFormat*> CreateCAudioFormat(
    StatusOr<AudioBufferCpp::AudioFormat> cpp_audio_format) {
  if (!cpp_audio_format.ok()) {
    return cpp_audio_format.status();
  }

  return new TfLiteAudioFormat{cpp_audio_format->channels,
                               cpp_audio_format->sample_rate};
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
