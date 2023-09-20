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

#include "tensorflow_lite_support/cc/task/processor/universal_sentence_encoder_preprocessor.h"

#include <initializer_list>
#include <memory>

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/processor/processor.h"

namespace tflite {
namespace task {
namespace processor {

namespace {

using ::tflite::support::StatusOr;
using ::tflite::task::core::PopulateTensor;
using ::tflite::task::core::TfLiteEngine;

constexpr int kQueryTextIndex = 0;
constexpr int kResponseContextIndex = 1;
constexpr int kResponseTextIndex = 2;

}  // namespace

/* static */
StatusOr<std::unique_ptr<UniversalSentenceEncoderPreprocessor>>
UniversalSentenceEncoderPreprocessor::Create(
    TfLiteEngine* engine,
    const std::initializer_list<int> input_tensor_indices) {
  return Processor::Create<UniversalSentenceEncoderPreprocessor>(
      /*num_expected_indices=*/3, engine, input_tensor_indices,
      /*requires_metadata=*/false);
}

absl::Status UniversalSentenceEncoderPreprocessor::Preprocess(
    const std::string& text) {
  // All input tensors must be populated, even though we're only using the
  // response text input tensor.
  TFLITE_RETURN_IF_ERROR(PopulateTensor(std::string(""), GetTensor(kQueryTextIndex)));
  TFLITE_RETURN_IF_ERROR(
      PopulateTensor(std::string(""), GetTensor(kResponseContextIndex)));
  TFLITE_RETURN_IF_ERROR(PopulateTensor(text, GetTensor(kResponseTextIndex)));
  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
