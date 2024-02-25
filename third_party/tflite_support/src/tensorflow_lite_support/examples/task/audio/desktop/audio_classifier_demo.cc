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

// Example usage:
// bazel run -c opt \
//  tensorflow_lite_support/examples/task/audio/desktop:audio_classifier_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --audio_wav_path=/path/to/audio.wav

#include <cstddef>
#include <iostream>
#include <limits>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "tensorflow_lite_support/examples/task/audio/desktop/audio_classifier_lib.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' audio classification model.");
ABSL_FLAG(std::string, audio_wav_path, "",
          "Absolute path to the 16-bit PCM WAV file to classify. The WAV "
          "file must be monochannel and has a sampling rate matches the model "
          "expected sampling rate (as in the Metadata).  If the WAV file is "
          "longer than what the model requires, only the beginning section is "
          "used for inference.");
ABSL_FLAG(float, score_threshold, 0.001f,
          "Apply a filter on the results. Only display classes with score "
          "higher than the threshold.");
ABSL_FLAG(bool, use_coral, false,
          "If true, inference will be delegated to a connected Coral Edge TPU "
          "device.");

int main(int argc, char** argv) {
  // Parse command line arguments and perform sanity checks.
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_model_path).empty()) {
    std::cerr << "Missing mandatory 'model_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_audio_wav_path).empty()) {
    std::cerr << "Missing mandatory 'audio_wav_path' argument.\n";
    return 1;
  }

  // Run classification.
  auto result = tflite::task::audio::Classify(
      absl::GetFlag(FLAGS_model_path), absl::GetFlag(FLAGS_audio_wav_path),
      absl::GetFlag(FLAGS_use_coral));
  if (result.ok()) {
    tflite::task::audio::Display(result.value(),
                                 absl::GetFlag(FLAGS_score_threshold));
  } else {
    std::cerr << "Classification failed: " << result.status().message() << "\n";
    return 1;
  }
}
