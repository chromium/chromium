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
#include "tensorflow_lite_support/cc/task/processor/regex_preprocessor.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"

namespace tflite {
namespace task {
namespace processor {

namespace {

using ::absl::StatusCode;
using ::tflite::TensorMetadata;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::RegexTokenizer;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::task::core::PopulateTensor;

StatusOr<absl::string_view> CheckAndLoadFirstAssociatedFile(
    const flatbuffers::Vector<flatbuffers::Offset<tflite::AssociatedFile>>*
        associated_files,
    const tflite::metadata::ModelMetadataExtractor* metadata_extractor) {
  if (associated_files == nullptr || associated_files->size() < 1 ||
      associated_files->Get(0)->name() == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Invalid vocab_file from input process unit.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  TFLITE_ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                   metadata_extractor->GetAssociatedFile(
                       associated_files->Get(0)->name()->str()));
  return vocab_buffer;
}

}  // namespace

/* static */
StatusOr<std::unique_ptr<RegexPreprocessor>> RegexPreprocessor::Create(
    tflite::task::core::TfLiteEngine* engine, int input_tensor_index) {
  TFLITE_ASSIGN_OR_RETURN(auto processor, Processor::Create<RegexPreprocessor>(
                                       /* num_expected_tensors = */ 1, engine,
                                       {input_tensor_index},
                                       /* requires_metadata = */ false));
  TFLITE_RETURN_IF_ERROR(processor->Init());
  return processor;
}

absl::Status RegexPreprocessor::Init() {
  // Check if the input is a STRING. If so, no tokenizer is needed.
  if (GetTensor()->type == kTfLiteString) {
    return absl::OkStatus();
  }
  // Try if RegexTokenzier metadata can be found.
  TFLITE_ASSIGN_OR_RETURN(const auto tokenzier_metadata,
                   TryFindRegexTokenizerMetadata());

  TFLITE_ASSIGN_OR_RETURN(tokenizer_, CreateTokenizerFromMetadata(
                                   tokenzier_metadata, GetMetadataExtractor()));
  return absl::OkStatus();
}

StatusOr<const tflite::ProcessUnit*>
RegexPreprocessor::TryFindRegexTokenizerMetadata() {
  // RegexTokenizer is packed in the processing unit of the input tensor.
  const TensorMetadata* tensor_metadata = GetTensorMetadata();
  if (tensor_metadata == nullptr) {
    return nullptr;
  }

  TFLITE_ASSIGN_OR_RETURN(
      auto tokenizer_metadata,
      GetMetadataExtractor()->FindFirstProcessUnit(
          *tensor_metadata, ProcessUnitOptions_RegexTokenizerOptions));

  if (tokenizer_metadata != nullptr) {
    // RegexTokenizer is found. Check if the tensor type matches.
    auto input_tensor = GetTensor();
    if (input_tensor->type != kTfLiteInt32) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrCat("Type mismatch for input tensor ", input_tensor->name,
                       ". Requested INT32 for RegexTokenizer, got ",
                       TfLiteTypeGetName(input_tensor->type), "."),
          TfLiteSupportStatus::kInvalidInputTensorTypeError);
    }
  }
  return tokenizer_metadata;
}

StatusOr<std::unique_ptr<RegexTokenizer>>
RegexPreprocessor::CreateTokenizerFromMetadata(
    const tflite::ProcessUnit* tokenizer_metadata,
    const tflite::metadata::ModelMetadataExtractor* metadata_extractor) {
  if (metadata_extractor == nullptr || tokenizer_metadata == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No metadata or input process unit found.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  if (tokenizer_metadata->options_type() ==
      ProcessUnitOptions_RegexTokenizerOptions) {
    const tflite::RegexTokenizerOptions* options =
        tokenizer_metadata->options_as<RegexTokenizerOptions>();
    TFLITE_ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                     CheckAndLoadFirstAssociatedFile(options->vocab_file(),
                                                     metadata_extractor));
    if (options->delim_regex_pattern() == nullptr) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "Invalid delim_regex_pattern from input process unit.",
          TfLiteSupportStatus::kMetadataInvalidTokenizerError);
    }

    std::unique_ptr<RegexTokenizer> regex_tokenizer =
        absl::make_unique<RegexTokenizer>(options->delim_regex_pattern()->str(),
                                          vocab_buffer.data(),
                                          vocab_buffer.size());

    int unknown_token_id = 0;
    if (!regex_tokenizer->GetUnknownToken(&unknown_token_id)) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "RegexTokenizer doesn't have <UNKNOWN> token.",
          TfLiteSupportStatus::kMetadataInvalidTokenizerError);
    }

    int pad_token_id = 0;
    if (!regex_tokenizer->GetPadToken(&pad_token_id)) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "RegexTokenizer doesn't have <PAD> token.",
          TfLiteSupportStatus::kMetadataInvalidTokenizerError);
    }

    return std::move(regex_tokenizer);
  } else {
    return CreateStatusWithPayload(
        absl::StatusCode::kNotFound,
        absl::StrCat("Incorrect options_type:",
                     tokenizer_metadata->options_type()),
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
}

absl::Status RegexPreprocessor::Preprocess(const std::string& input_text) {
  if (tokenizer_ == nullptr) {
    return PopulateTensor(input_text, GetTensor());
  } else {
    return RegexPreprocess(input_text);
  }
}

absl::Status RegexPreprocessor::RegexPreprocess(const std::string& input_text) {
  TfLiteTensor* input_tensor = GetTensor();

  //                              |<-------sentence_length-------->|
  // input_tensor                 <START>, t1, t2... <PAD>, <PAD>...
  // <START> is optional, t1, t2... will be replaced by <UNKNOWN> if it's
  // not found in tokenizer vocab.
  TokenizerResult result = tokenizer_->Tokenize(input_text);

  size_t max_sentence_length = input_tensor->dims->size == 2
                                   ? input_tensor->dims->data[1]
                                   : input_tensor->dims->data[0];

  int unknown_token_id = 0;
  tokenizer_->GetUnknownToken(&unknown_token_id);

  int pad_token_id = 0;
  tokenizer_->GetPadToken(&pad_token_id);

  std::vector<int> input_tokens(max_sentence_length, pad_token_id);
  int start_token_id = 0;
  size_t input_token_index = 0;
  if (tokenizer_->GetStartToken(&start_token_id)) {
    input_tokens[0] = start_token_id;
    input_token_index = 1;
  }

  for (size_t i = 0; (i < result.subwords.size()) &&
                     (input_token_index < max_sentence_length);
       ++i, ++input_token_index) {
    const std::string& token = result.subwords[i];
    int token_id = 0;
    if (tokenizer_->LookupId(token, &token_id)) {
      input_tokens[input_token_index] = token_id;
    } else {
      input_tokens[input_token_index] = unknown_token_id;
    }
  }
  return PopulateTensor(input_tokens, input_tensor);
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
