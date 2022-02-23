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
#include "tensorflow_lite_support/cc/task/processor/text_preprocessor.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/processor/bert_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/regex_preprocessor.h"

namespace tflite {
namespace task {
namespace processor {

namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;

}  // namespace

/* static */ StatusOr<std::unique_ptr<TextPreprocessor>>
TextPreprocessor::Create(
    tflite::task::core::TfLiteEngine* engine,
    const std::initializer_list<int> input_tensor_indices) {
  switch (input_tensor_indices.size()) {
    case 1:
      return RegexPreprocessor::Create(engine, *input_tensor_indices.begin());
    case 3:
      return BertPreprocessor::Create(engine, input_tensor_indices);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "TextPreprocessor accepts either 1 input tensor (for Regex "
              "tokenizer or String tensor) or 3 input tensors (for Bert "
              "tokenizer), but got %d tensors.",
              input_tensor_indices.size()));
  }
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
