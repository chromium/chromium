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

#include "tensorflow_lite_support/cc/task/text/nlclassifier/bert_nl_classifier.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
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
namespace nlclassifier {

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

  std::string processed_input = input;
  absl::AsciiStrToLower(&processed_input);

  TokenizerResult input_tokenize_results;
  input_tokenize_results = tokenizer_->Tokenize(processed_input);

  // 2 accounts for [CLS], [SEP]
  absl::Span<const std::string> query_tokens =
      absl::MakeSpan(input_tokenize_results.subwords.data(),
                     input_tokenize_results.subwords.data() +
                         std::min(static_cast<size_t>(kMaxSeqLen - 2),
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

  std::vector<int> input_ids(kMaxSeqLen, 0);
  std::vector<int> input_mask(kMaxSeqLen, 0);
  // Convert tokens back into ids and set mask
  for (int i = 0; i < tokens.size(); ++i) {
    tokenizer_->LookupId(tokens[i], &input_ids[i]);
    input_mask[i] = 1;
  }
  //                             |<-----------kMaxSeqLen---------->|
  // input_ids                 [CLS] s1  s2...  sn [SEP]  0  0...  0
  // input_masks                 1    1   1...  1    1    0  0...  0
  // segment_ids                 0    0   0...  0    0    0  0...  0

  PopulateTensor(input_ids, ids_tensor);
  PopulateTensor(input_mask, mask_tensor);
  PopulateTensor(std::vector<int>(kMaxSeqLen, 0), segment_ids_tensor);

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

StatusOr<std::unique_ptr<BertNLClassifier>> BertNLClassifier::CreateFromFile(
    const std::string& path_to_model_with_metadata,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<BertNLClassifier> bert_nl_classifier;
  ASSIGN_OR_RETURN(bert_nl_classifier,
                   core::TaskAPIFactory::CreateFromFile<BertNLClassifier>(
                       path_to_model_with_metadata, std::move(resolver)));
  RETURN_IF_ERROR(bert_nl_classifier->InitializeFromMetadata());
  return std::move(bert_nl_classifier);
}

StatusOr<std::unique_ptr<BertNLClassifier>> BertNLClassifier::CreateFromBuffer(
    const char* model_with_metadata_buffer_data,
    size_t model_with_metadata_buffer_size,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<BertNLClassifier> bert_nl_classifier;
  ASSIGN_OR_RETURN(bert_nl_classifier,
                   core::TaskAPIFactory::CreateFromBuffer<BertNLClassifier>(
                       model_with_metadata_buffer_data,
                       model_with_metadata_buffer_size, std::move(resolver)));
  RETURN_IF_ERROR(bert_nl_classifier->InitializeFromMetadata());
  return std::move(bert_nl_classifier);
}

StatusOr<std::unique_ptr<BertNLClassifier>> BertNLClassifier::CreateFromFd(
    int fd,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<BertNLClassifier> bert_nl_classifier;
  ASSIGN_OR_RETURN(
      bert_nl_classifier,
      core::TaskAPIFactory::CreateFromFileDescriptor<BertNLClassifier>(
          fd, std::move(resolver)));
  RETURN_IF_ERROR(bert_nl_classifier->InitializeFromMetadata());
  return std::move(bert_nl_classifier);
}

absl::Status BertNLClassifier::InitializeFromMetadata() {
  // Set up mandatory tokenizer.
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

  // Set up optional label vector.
  TrySetLabelFromMetadata(
      GetMetadataExtractor()->GetOutputTensorMetadata(kOutputTensorIndex))
      .IgnoreError();
  return absl::OkStatus();
}

}  // namespace nlclassifier
}  // namespace text
}  // namespace task
}  // namespace tflite
