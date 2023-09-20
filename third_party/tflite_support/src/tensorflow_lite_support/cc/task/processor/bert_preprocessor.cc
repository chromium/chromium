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
#include "tensorflow_lite_support/cc/task/processor/bert_preprocessor.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/ascii.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"
#include "tensorflow_lite_support/cc/utils/common_utils.h"

namespace tflite {
namespace task {
namespace processor {

using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::CreateTokenizerFromProcessUnit;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::task::core::PopulateTensor;

constexpr int kTokenizerProcessUnitIndex = 0;
constexpr int kIdsTensorIndex = 0;
constexpr int kSegmentIdsTensorIndex = 1;
constexpr int kMaskTensorIndex = 2;
constexpr char kClassificationToken[] = "[CLS]";
constexpr char kSeparator[] = "[SEP]";

/* static */
StatusOr<std::unique_ptr<BertPreprocessor>> BertPreprocessor::Create(
    tflite::task::core::TfLiteEngine* engine,
    const std::initializer_list<int> input_tensor_indices) {
  TFLITE_ASSIGN_OR_RETURN(auto processor, Processor::Create<BertPreprocessor>(
                                       /* num_expected_tensors = */ 3, engine,
                                       input_tensor_indices,
                                       /* requires_metadata = */ false));
  TFLITE_RETURN_IF_ERROR(processor->Init());
  return processor;
}

// TODO(b/241507692) Add a unit test for a model with dynamic tensors.
absl::Status BertPreprocessor::Init() {
  // Try if RegexTokenizer can be found.
  // BertTokenizer is packed in the processing unit SubgraphMetadata.
  const tflite::ProcessUnit* tokenizer_metadata =
      GetMetadataExtractor()->GetInputProcessUnit(kTokenizerProcessUnitIndex);
  TFLITE_ASSIGN_OR_RETURN(tokenizer_, CreateTokenizerFromProcessUnit(
                                   tokenizer_metadata, GetMetadataExtractor()));

  const auto& ids_tensor = *GetTensor(kIdsTensorIndex);
  const auto& mask_tensor = *GetTensor(kMaskTensorIndex);
  const auto& segment_ids_tensor = *GetTensor(kSegmentIdsTensorIndex);
  if (ids_tensor.dims->size != 2 || mask_tensor.dims->size != 2 ||
      segment_ids_tensor.dims->size != 2) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "The three input tensors in Bert models are expected to have dim "
            "2, but got ids_tensor (%d), mask_tensor (%d), segment_ids_tensor "
            "(%d).",
            ids_tensor.dims->size, mask_tensor.dims->size,
            segment_ids_tensor.dims->size),
        TfLiteSupportStatus::kInvalidInputTensorDimensionsError);
  }
  if (ids_tensor.dims->data[0] != 1 || mask_tensor.dims->data[0] != 1 ||
      segment_ids_tensor.dims->data[0] != 1) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "The three input tensors in Bert models are expected to have same "
            "batch size 1, but got ids_tensor (%d), mask_tensor (%d), "
            "segment_ids_tensor (%d).",
            ids_tensor.dims->data[0], mask_tensor.dims->data[0],
            segment_ids_tensor.dims->data[0]),
        TfLiteSupportStatus::kInvalidInputTensorSizeError);
  }
  if (ids_tensor.dims->data[1] != mask_tensor.dims->data[1] ||
      ids_tensor.dims->data[1] != segment_ids_tensor.dims->data[1]) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat("The three input tensors in Bert models are "
                        "expected to have same length, but got ids_tensor "
                        "(%d), mask_tensor (%d), segment_ids_tensor (%d).",
                        ids_tensor.dims->data[1], mask_tensor.dims->data[1],
                        segment_ids_tensor.dims->data[1]),
        TfLiteSupportStatus::kInvalidInputTensorSizeError);
  }

  bool has_valid_dims_signature = ids_tensor.dims_signature->size == 2 &&
                                  mask_tensor.dims_signature->size == 2 &&
                                  segment_ids_tensor.dims_signature->size == 2;
  if (has_valid_dims_signature && ids_tensor.dims_signature->data[1] == -1 &&
      mask_tensor.dims_signature->data[1] == -1 &&
      segment_ids_tensor.dims_signature->data[1] == -1) {
    input_tensors_are_dynamic_ = true;
  } else if (has_valid_dims_signature &&
             (ids_tensor.dims_signature->data[1] == -1 ||
              mask_tensor.dims_signature->data[1] == -1 ||
              segment_ids_tensor.dims_signature->data[1] == -1)) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        "Input tensors contain a mix of static and dynamic tensors",
        TfLiteSupportStatus::kInvalidInputTensorSizeError);
  }

  if (input_tensors_are_dynamic_) return absl::OkStatus();

  bert_max_seq_len_ = ids_tensor.dims->data[1];
  if (bert_max_seq_len_ < 2) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat("bert_max_seq_len_ should be at least 2, got: (%d).",
                        bert_max_seq_len_),
        TfLiteSupportStatus::kInvalidInputTensorSizeError);
  }
  return absl::OkStatus();
}

absl::Status BertPreprocessor::Preprocess(const std::string& input_text) {
  auto* ids_tensor = GetTensor(kIdsTensorIndex);
  auto* mask_tensor = GetTensor(kMaskTensorIndex);
  auto* segment_ids_tensor = GetTensor(kSegmentIdsTensorIndex);

  std::string processed_input = input_text;
  absl::AsciiStrToLower(&processed_input);

  TokenizerResult input_tokenize_results;
  input_tokenize_results = tokenizer_->Tokenize(processed_input);

  // Offset by 2 to account for [CLS] and [SEP]
  int input_tokens_size =
      static_cast<int>(input_tokenize_results.subwords.size()) + 2;
  int input_tensor_length = input_tokens_size;
  if (!input_tensors_are_dynamic_) {
    input_tokens_size = std::min(bert_max_seq_len_, input_tokens_size);
    input_tensor_length = bert_max_seq_len_;
  } else {
    engine_->interpreter()->ResizeInputTensorStrict(kIdsTensorIndex,
                                                    {1, input_tensor_length});
    engine_->interpreter()->ResizeInputTensorStrict(kMaskTensorIndex,
                                                    {1, input_tensor_length});
    engine_->interpreter()->ResizeInputTensorStrict(kSegmentIdsTensorIndex,
                                                    {1, input_tensor_length});
    engine_->interpreter()->AllocateTensors();
  }

  std::vector<std::string> input_tokens;
  input_tokens.reserve(input_tokens_size);
  input_tokens.push_back(std::string(kClassificationToken));
  for (int i = 0; i < input_tokens_size - 2; ++i) {
    input_tokens.push_back(std::move(input_tokenize_results.subwords[i]));
  }
  input_tokens.push_back(std::string(kSeparator));

  std::vector<int> input_ids(input_tensor_length, 0);
  std::vector<int> input_mask(input_tensor_length, 0);
  // Convert tokens back into ids and set mask
  for (int i = 0; i < input_tokens.size(); ++i) {
    tokenizer_->LookupId(input_tokens[i], &input_ids[i]);
    input_mask[i] = 1;
  }
  //                           |<--------input_tensor_length------->|
  // input_ids                 [CLS] s1  s2...  sn [SEP]  0  0...  0
  // input_masks                 1    1   1...  1    1    0  0...  0
  // segment_ids                 0    0   0...  0    0    0  0...  0

  TFLITE_RETURN_IF_ERROR(PopulateTensor(input_ids, ids_tensor));
  TFLITE_RETURN_IF_ERROR(PopulateTensor(input_mask, mask_tensor));
  TFLITE_RETURN_IF_ERROR(PopulateTensor(std::vector<int>(input_tensor_length, 0),
                                 segment_ids_tensor));
  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
