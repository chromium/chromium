/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"       // from @com_google_absl
#include "absl/strings/ascii.h"       // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow/lite/string_type.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/category.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"

namespace tflite {
namespace task {
namespace text {

using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::CreateTokenizerFromProcessUnit;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::task::core::FindTensorByName;
using ::tflite::task::core::PopulateTensor;

namespace {
constexpr char kIdsTensorName[] = "ids";
constexpr char kMaskTensorName[] = "mask";
constexpr char kSegmentIdsTensorName[] = "segment_ids";
constexpr char kScoreTensorName[] = "probability";
constexpr char kClassificationToken[] = "[CLS]";
constexpr char kSeparator[] = "[SEP]";
constexpr int kTokenizerProcessUnitIndex = 0;

absl::Status SanityCheckOptions(const BertNLClassifierOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(absl::StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

int GetLastDimSize(const TfLiteTensor* tensor) {
  return tensor->dims->data[tensor->dims->size - 1];
}

}  // namespace

absl::Status BertNLClassifier::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::string& input) {
  auto* input_tensor_metadatas =
      GetMetadataExtractor()->GetInputTensorMetadata();
  auto* ids_tensor =
      FindTensorByName(input_tensors, input_tensor_metadatas, kIdsTensorName);
  auto* mask_tensor =
      FindTensorByName(input_tensors, input_tensor_metadatas, kMaskTensorName);
  auto* segment_ids_tensor = FindTensorByName(
      input_tensors, input_tensor_metadatas, kSegmentIdsTensorName);

  if (GetLastDimSize(ids_tensor) != GetLastDimSize(mask_tensor) ||
      GetLastDimSize(ids_tensor) != GetLastDimSize(segment_ids_tensor)) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat("The three input tensors in BertNLClassifier models "
                        "are expected to have same length, but got ids_tensor "
                        "(%d), mask_tensor (%d), segment_ids_tensor (%d).",
                        GetLastDimSize(ids_tensor), GetLastDimSize(mask_tensor),
                        GetLastDimSize(ids_tensor)),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }

  int max_seq_len = GetLastDimSize(ids_tensor);

  std::string processed_input = input;
  absl::AsciiStrToLower(&processed_input);

  TokenizerResult input_tokenize_results;
  input_tokenize_results = tokenizer_->Tokenize(processed_input);

  // 2 accounts for [CLS], [SEP]
  absl::Span<const std::string> query_tokens =
      absl::MakeSpan(input_tokenize_results.subwords.data(),
                     input_tokenize_results.subwords.data() +
                         std::min(static_cast<size_t>(max_seq_len - 2),
                                  input_tokenize_results.subwords.size()));

  std::vector<std::string> tokens;
  tokens.reserve(2 + query_tokens.size());
  // Start of generating the features.
  tokens.push_back(kClassificationToken);
  // For query input.
  for (const auto& query_token : query_tokens) {
    tokens.push_back(query_token);
  }
  // For Separation.
  tokens.push_back(kSeparator);

  std::vector<int> input_ids(max_seq_len, 0);
  std::vector<int> input_mask(max_seq_len, 0);
  // Convert tokens back into ids and set mask
  for (int i = 0; i < tokens.size(); ++i) {
    tokenizer_->LookupId(tokens[i], &input_ids[i]);
    input_mask[i] = 1;
  }
  //                             |<-----------max_seq_len--------->|
  // input_ids                 [CLS] s1  s2...  sn [SEP]  0  0...  0
  // input_masks                 1    1   1...  1    1    0  0...  0
  // segment_ids                 0    0   0...  0    0    0  0...  0

  RETURN_IF_ERROR(PopulateTensor(input_ids, ids_tensor));
  RETURN_IF_ERROR(PopulateTensor(input_mask, mask_tensor));
  RETURN_IF_ERROR(
      PopulateTensor(std::vector<int>(max_seq_len, 0), segment_ids_tensor));

  return absl::OkStatus();
}

StatusOr<std::vector<core::Category>> BertNLClassifier::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::string& /*input*/) {
  if (output_tensors.size() != 1) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("BertNLClassifier models are expected to have only 1 "
                        "output, found %d",
                        output_tensors.size()),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }
  const TfLiteTensor* scores = FindTensorByName(
      output_tensors, GetMetadataExtractor()->GetOutputTensorMetadata(),
      kScoreTensorName);

  // optional labels extracted from metadata
  return BuildResults(scores, /*labels=*/nullptr);
}

StatusOr<std::unique_ptr<BertNLClassifier>> BertNLClassifier::CreateFromOptions(
    const BertNLClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  RETURN_IF_ERROR(SanityCheckOptions(options));

  auto options_copy = absl::make_unique<BertNLClassifierOptions>(options);

  ASSIGN_OR_RETURN(
      auto bert_nl_classifier,
      core::TaskAPIFactory::CreateFromBaseOptions<BertNLClassifier>(
          &options_copy->base_options(), std::move(resolver)));
  RETURN_IF_ERROR(bert_nl_classifier->Initialize(std::move(options_copy)));
  return std::move(bert_nl_classifier);
}

absl::Status BertNLClassifier::Initialize(
    std::unique_ptr<BertNLClassifierOptions> options) {
  options_ = std::move(options);
  // Set up mandatory tokenizer from metadata.
  const ProcessUnit* tokenizer_process_unit =
      GetMetadataExtractor()->GetInputProcessUnit(kTokenizerProcessUnitIndex);
  if (tokenizer_process_unit == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No input process unit found from metadata.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  ASSIGN_OR_RETURN(tokenizer_,
                   CreateTokenizerFromProcessUnit(tokenizer_process_unit,
                                                  GetMetadataExtractor()));

  // Set up optional label vector from metadata.
  TrySetLabelFromMetadata(
      GetMetadataExtractor()->GetOutputTensorMetadata(kOutputTensorIndex))
      .IgnoreError();

  return absl::OkStatus();
}

}  // namespace text
}  // namespace task
}  // namespace tflite
