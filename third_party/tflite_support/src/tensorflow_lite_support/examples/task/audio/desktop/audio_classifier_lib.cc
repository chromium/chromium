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

#include "tensorflow_lite_support/examples/task/audio/desktop/audio_classifier_lib.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/audio/audio_classifier.h"
#include "tensorflow_lite_support/cc/task/audio/core/audio_buffer.h"
#include "tensorflow_lite_support/cc/task/audio/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/audio/utils/wav_io.h"
#include "tensorflow_lite_support/cc/task/core/category.h"

namespace tflite {
namespace task {
namespace audio {

tflite::support::StatusOr<AudioBuffer> LoadAudioBufferFromFile(
    const std::string& wav_file, uint32_t* buffer_size, uint32_t* offset,
    std::vector<float>* wav_data) {
  std::string contents = ReadFile(wav_file);

  uint32 decoded_sample_count;
  uint16 decoded_channel_count;
  uint32 decoded_sample_rate;
  TFLITE_RETURN_IF_ERROR(DecodeLin16WaveAsFloatVector(
      contents, wav_data, offset, &decoded_sample_count, &decoded_channel_count,
      &decoded_sample_rate));

  if (decoded_sample_count > *buffer_size) {
    decoded_sample_count = *buffer_size;
  }

  return AudioBuffer(
      wav_data->data(), decoded_sample_count,
      {decoded_channel_count, static_cast<int>(decoded_sample_rate)});
}

tflite::support::StatusOr<ClassificationResult> Classify(
    const std::string& model_path, const std::string& wav_file,
    bool use_coral) {
  AudioClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      model_path);
  if (use_coral) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<AudioClassifier> classifier,
                   AudioClassifier::CreateFromOptions(options));

  // `wav_data` holds data loaded from the file and needs to outlive `buffer`.
  std::vector<float> wav_data;
  uint32_t offset = 0;
  uint32_t buffer_size = classifier->GetRequiredInputBufferSize();
  TFLITE_ASSIGN_OR_RETURN(
      AudioBuffer buffer,
      LoadAudioBufferFromFile(wav_file, &buffer_size, &offset, &wav_data));

  auto start_classify = std::chrono::steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(ClassificationResult result, classifier->Classify(buffer));
  auto end_classify = std::chrono::steady_clock::now();
  std::string delegate = use_coral ? "Coral Edge TPU" : "CPU";
  const auto duration_ms =
      std::chrono::duration<float, std::milli>(end_classify - start_classify);
  std::cout << "Time cost to classify the input audio clip on " << delegate
            << ": " << duration_ms.count() << " ms" << std::endl;

  return result;
}

void Display(const ClassificationResult& result, float score_threshold) {
  std::cout << "Note: Only showing classes with score higher than "
            << score_threshold << std::endl;

  for (int i = 0; i < result.classifications_size(); i++) {
    const auto& head = result.classifications(i);
    std::cout << absl::StrFormat("\nHead[%d]: %s\n", i, head.head_name());
    for (int j = 0; j < head.classes_size(); j++) {
      const auto& category = head.classes(j);
      if (category.score() < score_threshold) continue;
      std::cout << absl::StrFormat("\tcategory[%s]: %.5f\t",
                                   category.class_name(), category.score());
      if (!category.display_name().empty()) {
        std::cout << category.display_name();
      }
      std::cout << std::endl;
    }
  }
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
