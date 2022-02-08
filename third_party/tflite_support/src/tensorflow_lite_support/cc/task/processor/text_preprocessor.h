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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_TEXT_PREPROCESSOR_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_TEXT_PREPROCESSOR_H_

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/processor/processor.h"

namespace tflite {
namespace task {
namespace processor {

// Processes input text and populates the associated input tensors.
// Requirements for the input tensors (either one of the following):
//   - One input tensor:
//     A string tensor of type, kTfLiteString
//        or
//     An int32 tensor of type, kTfLiteInt32: contains the tokenized indices of
//     a string input. A RegexTokenizer needs to be set up in the input tensor's
//     metadata.
//   - Three input tensors (input tensors of a Bert model):
//     The 3 input tensors should be populated with metadata tensor names,
//     "ids", "mask", and "segment_ids", respectively. The input_process_units
//     metadata should contain WordPiece or Sentencepiece Tokenizer
//     metadata.
class TextPreprocessor : public Preprocessor {
 public:
  static tflite::support::StatusOr<std::unique_ptr<TextPreprocessor>> Create(
      tflite::task::core::TfLiteEngine* engine,
      const std::initializer_list<int> input_tensor_indices);

  virtual absl::Status Preprocess(const std::string& text) = 0;

 protected:
  using Preprocessor::Preprocessor;
};

}  // namespace processor
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_TEXT_PREPROCESSOR_H_
