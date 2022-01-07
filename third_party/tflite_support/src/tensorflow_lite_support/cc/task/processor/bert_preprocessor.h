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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_BERT_PREPROCESOR_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_BERT_PREPROCESOR_H_

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/processor/text_preprocessor.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer.h"

namespace tflite {
namespace task {
namespace processor {

// Processes input text and populates the associated bert input tensors.
// Requirements for the input tensors:
//   - The 3 input tensors should be populated with the metadata tensor names,
//   "ids", "mask", and "segment_ids", respectively.
//   - The input_process_units metadata should contain WordPiece or
//   Sentencepiece Tokenizer metadata.
class BertPreprocessor : public TextPreprocessor {
 public:
  static tflite::support::StatusOr<std::unique_ptr<BertPreprocessor>> Create(
      tflite::task::core::TfLiteEngine* engine,
      const std::initializer_list<int> input_tensor_indices);

  absl::Status Preprocess(const std::string& text);

 private:
  using TextPreprocessor::TextPreprocessor;

  absl::Status Init();

  int GetLastDimSize(int tensor_index);

  std::unique_ptr<tflite::support::text::tokenizer::Tokenizer> tokenizer_;
  int ids_tensor_index_;
  int mask_tensor_index_;
  int segment_ids_tensor_index_;
  int bert_max_seq_len_;
};

}  // namespace processor
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_PROCESSOR_BERT_PREPROCESOR_H_
