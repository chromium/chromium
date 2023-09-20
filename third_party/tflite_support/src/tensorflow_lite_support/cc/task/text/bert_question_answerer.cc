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

#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_join.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace text {

constexpr char kIdsTensorName[] = "ids";
constexpr char kMaskTensorName[] = "mask";
constexpr char kSegmentIdsTensorName[] = "segment_ids";
constexpr char kEndLogitsTensorName[] = "end_logits";
constexpr char kStartLogitsTensorName[] = "start_logits";

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::BertTokenizer;
using ::tflite::support::text::tokenizer::CreateTokenizerFromProcessUnit;
using ::tflite::support::text::tokenizer::SentencePieceTokenizer;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::task::core::FindTensorByName;
using ::tflite::task::core::PopulateTensor;
using ::tflite::task::core::PopulateVector;
using ::tflite::task::core::ReverseSortIndices;

namespace {
constexpr int kTokenizerProcessUnitIndex = 0;

absl::Status SanityCheckOptions(const BertQuestionAnswererOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}
}  // namespace

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateFromOptions(
    const BertQuestionAnswererOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the duration of this
  // created BertQuestionAnswerer object.
  auto options_copy = absl::make_unique<BertQuestionAnswererOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(
      auto bert_question_answerer,
      core::TaskAPIFactory::CreateFromBaseOptions<BertQuestionAnswerer>(
          &options_copy->base_options(), std::move(resolver)));
  TFLITE_RETURN_IF_ERROR(
      bert_question_answerer->InitializeFromMetadata(std::move(options_copy)));
  return std::move(bert_question_answerer);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateFromFile(
    const std::string& path_to_model_with_metadata) {
  BertQuestionAnswererOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      path_to_model_with_metadata);
  return CreateFromOptions(options);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateFromBuffer(
    const char* model_with_metadata_buffer_data,
    size_t model_with_metadata_buffer_size) {
  BertQuestionAnswererOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_content(
      model_with_metadata_buffer_data, model_with_metadata_buffer_size);
  return CreateFromOptions(options);
}

StatusOr<std::unique_ptr<QuestionAnswerer>> BertQuestionAnswerer::CreateFromFd(
    int fd) {
  BertQuestionAnswererOptions options;
  options.mutable_base_options()
      ->mutable_model_file()
      ->mutable_file_descriptor_meta()
      ->set_fd(fd);
  return CreateFromOptions(options);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateBertQuestionAnswererFromFile(
    const std::string& path_to_model, const std::string& path_to_vocab) {
  std::unique_ptr<BertQuestionAnswerer> api_to_init;
  TFLITE_ASSIGN_OR_RETURN(
      api_to_init,
      core::TaskAPIFactory::CreateFromFile<BertQuestionAnswerer>(
          path_to_model,
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
          kNumLiteThreads));
  api_to_init->InitializeBertTokenizer(path_to_vocab);
  return std::move(api_to_init);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateBertQuestionAnswererFromBuffer(
    const char* model_buffer_data, size_t model_buffer_size,
    const char* vocab_buffer_data, size_t vocab_buffer_size) {
  std::unique_ptr<BertQuestionAnswerer> api_to_init;
  TFLITE_ASSIGN_OR_RETURN(
      api_to_init,
      core::TaskAPIFactory::CreateFromBuffer<BertQuestionAnswerer>(
          model_buffer_data, model_buffer_size,
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
          kNumLiteThreads));
  api_to_init->InitializeBertTokenizerFromBinary(vocab_buffer_data,
                                                 vocab_buffer_size);
  return std::move(api_to_init);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateAlbertQuestionAnswererFromFile(
    const std::string& path_to_model, const std::string& path_to_spmodel) {
  std::unique_ptr<BertQuestionAnswerer> api_to_init;
  TFLITE_ASSIGN_OR_RETURN(
      api_to_init,
      core::TaskAPIFactory::CreateFromFile<BertQuestionAnswerer>(
          path_to_model,
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
          kNumLiteThreads));
  api_to_init->InitializeSentencepieceTokenizer(path_to_spmodel);
  return std::move(api_to_init);
}

StatusOr<std::unique_ptr<QuestionAnswerer>>
BertQuestionAnswerer::CreateAlbertQuestionAnswererFromBuffer(
    const char* model_buffer_data, size_t model_buffer_size,
    const char* spmodel_buffer_data, size_t spmodel_buffer_size) {
  std::unique_ptr<BertQuestionAnswerer> api_to_init;
  TFLITE_ASSIGN_OR_RETURN(
      api_to_init,
      core::TaskAPIFactory::CreateFromBuffer<BertQuestionAnswerer>(
          model_buffer_data, model_buffer_size,
          absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>(),
          kNumLiteThreads));
  api_to_init->InitializeSentencepieceTokenizerFromBinary(spmodel_buffer_data,
                                                          spmodel_buffer_size);
  return std::move(api_to_init);
}

std::vector<QaAnswer> BertQuestionAnswerer::Answer(
    const std::string& context, const std::string& question) {
  // The BertQuestionAnswererer implementation for Preprocess() and
  // Postprocess() never returns errors: just call value().
  return Infer(context, question).value();
}

absl::Status BertQuestionAnswerer::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors, const std::string& context,
    const std::string& query) {
  auto* input_tensor_metadatas =
      GetMetadataExtractor()->GetInputTensorMetadata();
  TfLiteTensor* ids_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kIdsTensorName)
          : input_tensors[0];
  TfLiteTensor* mask_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kMaskTensorName)
          : input_tensors[1];
  TfLiteTensor* segment_ids_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kSegmentIdsTensorName)
          : input_tensors[2];

  token_to_orig_map_.clear();

  // The orig_tokens is used for recovering the answer string from the index,
  // while the processed_tokens is lower-cased and used to generate input of
  // the model.
  orig_tokens_ = absl::StrSplit(context, absl::ByChar(' '), absl::SkipEmpty());
  std::vector<std::string> processed_tokens(orig_tokens_);

  std::string processed_query = query;
  if (kUseLowerCase) {
    for (auto& token : processed_tokens) {
      absl::AsciiStrToLower(&token);
    }
    absl::AsciiStrToLower(&processed_query);
  }

  TokenizerResult query_tokenize_results;
  query_tokenize_results = tokenizer_->Tokenize(processed_query);

  std::vector<std::string> query_tokens = query_tokenize_results.subwords;
  if (query_tokens.size() > kMaxQueryLen) {
    query_tokens.resize(kMaxQueryLen);
  }

  // Example:
  // context:             tokenize     me  please
  // all_doc_tokens:      token ##ize  me  plea ##se
  // token_to_orig_index: [0,   0,     1,  2,   2]

  std::vector<std::string> all_doc_tokens;
  std::vector<int> token_to_orig_index;
  for (size_t i = 0; i < processed_tokens.size(); i++) {
    const std::string& token = processed_tokens[i];
    std::vector<std::string> sub_tokens = tokenizer_->Tokenize(token).subwords;
    for (const std::string& sub_token : sub_tokens) {
      token_to_orig_index.emplace_back(i);
      all_doc_tokens.emplace_back(sub_token);
    }
  }

  // -3 accounts for [CLS], [SEP] and [SEP].
  int max_context_len = kMaxSeqLen - query_tokens.size() - 3;
  if (all_doc_tokens.size() > max_context_len) {
    all_doc_tokens.resize(max_context_len);
  }

  std::vector<std::string> tokens;
  tokens.reserve(3 + query_tokens.size() + all_doc_tokens.size());
  std::vector<int> segment_ids;
  segment_ids.reserve(kMaxSeqLen);

  // Start of generating the features.
  tokens.emplace_back("[CLS]");
  segment_ids.emplace_back(0);

  // For query input.
  for (const auto& query_token : query_tokens) {
    tokens.emplace_back(query_token);
    segment_ids.emplace_back(0);
  }

  // For Separation.
  tokens.emplace_back("[SEP]");
  segment_ids.emplace_back(0);

  // For Text Input.
  for (int i = 0; i < all_doc_tokens.size(); i++) {
    auto& doc_token = all_doc_tokens[i];
    tokens.emplace_back(doc_token);
    segment_ids.emplace_back(1);
    token_to_orig_map_[tokens.size()] = token_to_orig_index[i];
  }

  // For ending mark.
  tokens.emplace_back("[SEP]");
  segment_ids.emplace_back(1);

  std::vector<int> input_ids(tokens.size());
  input_ids.reserve(kMaxSeqLen);
  // Convert tokens back into ids
  for (int i = 0; i < tokens.size(); i++) {
    auto& token = tokens[i];
    tokenizer_->LookupId(token, &input_ids[i]);
  }

  std::vector<int> input_mask;
  input_mask.reserve(kMaxSeqLen);
  input_mask.insert(input_mask.end(), tokens.size(), 1);

  int zeros_to_pad = kMaxSeqLen - input_ids.size();
  input_ids.insert(input_ids.end(), zeros_to_pad, 0);
  input_mask.insert(input_mask.end(), zeros_to_pad, 0);
  segment_ids.insert(segment_ids.end(), zeros_to_pad, 0);

  // input_ids INT32[1, 384]
  TFLITE_RETURN_IF_ERROR(PopulateTensor(input_ids, ids_tensor));
  // input_mask INT32[1, 384]
  TFLITE_RETURN_IF_ERROR(PopulateTensor(input_mask, mask_tensor));
  // segment_ids INT32[1, 384]
  TFLITE_RETURN_IF_ERROR(PopulateTensor(segment_ids, segment_ids_tensor));

  return absl::OkStatus();
}

StatusOr<std::vector<QaAnswer>> BertQuestionAnswerer::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::string& /*lowercased_context*/,
    const std::string& /*lowercased_query*/) {
  auto* output_tensor_metadatas =
      GetMetadataExtractor()->GetOutputTensorMetadata();

  const TfLiteTensor* end_logits_tensor =
      output_tensor_metadatas
          ? FindTensorByName(output_tensors, output_tensor_metadatas,
                             kEndLogitsTensorName)
          : output_tensors[0];
  const TfLiteTensor* start_logits_tensor =
      output_tensor_metadatas
          ? FindTensorByName(output_tensors, output_tensor_metadatas,
                             kStartLogitsTensorName)
          : output_tensors[1];

  std::vector<float> end_logits;
  std::vector<float> start_logits;

  // end_logits FLOAT[1, 384]
  TFLITE_RETURN_IF_ERROR(PopulateVector(end_logits_tensor, &end_logits));
  // start_logits FLOAT[1, 384]
  TFLITE_RETURN_IF_ERROR(PopulateVector(start_logits_tensor, &start_logits));

  auto start_indices = ReverseSortIndices(start_logits);
  auto end_indices = ReverseSortIndices(end_logits);

  std::vector<QaAnswer::Pos> orig_results;
  for (int start_index = 0; start_index < kPredictAnsNum; start_index++) {
    for (int end_index = 0; end_index < kPredictAnsNum; end_index++) {
      int start = start_indices[start_index];
      int end = end_indices[end_index];

      if (!token_to_orig_map_.contains(start + kOutputOffset) ||
          !token_to_orig_map_.contains(end + kOutputOffset) || end < start ||
          (end - start + 1) > kMaxAnsLen) {
        continue;
      }
      orig_results.emplace_back(
          QaAnswer::Pos(start, end, start_logits[start] + end_logits[end]));
    }
  }

  std::sort(orig_results.begin(), orig_results.end());

  std::vector<QaAnswer> answers;
  for (int i = 0; i < orig_results.size() && i < kPredictAnsNum; i++) {
    auto orig_pos = orig_results[i];
    answers.emplace_back(
        orig_pos.start > 0 ? ConvertIndexToString(orig_pos.start, orig_pos.end)
                           : "",
        orig_pos);
  }

  return answers;
}

std::string BertQuestionAnswerer::ConvertIndexToString(int start, int end) {
  int start_index = token_to_orig_map_[start + kOutputOffset];
  int end_index = token_to_orig_map_[end + kOutputOffset];

  return absl::StrJoin(orig_tokens_.begin() + start_index,
                       orig_tokens_.begin() + end_index + 1, " ");
}

absl::Status BertQuestionAnswerer::InitializeFromMetadata(
    std::unique_ptr<BertQuestionAnswererOptions> options) {
  options_ = std::move(options);

  const ProcessUnit* tokenizer_process_unit =
      GetMetadataExtractor()->GetInputProcessUnit(kTokenizerProcessUnitIndex);
  if (tokenizer_process_unit == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No input process unit found from metadata.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  TFLITE_ASSIGN_OR_RETURN(tokenizer_,
                   CreateTokenizerFromProcessUnit(tokenizer_process_unit,
                                                  GetMetadataExtractor()));
  return absl::OkStatus();
}

void BertQuestionAnswerer::InitializeBertTokenizer(
    const std::string& path_to_vocab) {
  tokenizer_ = absl::make_unique<BertTokenizer>(path_to_vocab);
}

void BertQuestionAnswerer::InitializeBertTokenizerFromBinary(
    const char* vocab_buffer_data, size_t vocab_buffer_size) {
  tokenizer_ =
      absl::make_unique<BertTokenizer>(vocab_buffer_data, vocab_buffer_size);
}

void BertQuestionAnswerer::InitializeSentencepieceTokenizer(
    const std::string& path_to_spmodel) {
  tokenizer_ = absl::make_unique<SentencePieceTokenizer>(path_to_spmodel);
}

void BertQuestionAnswerer::InitializeSentencepieceTokenizerFromBinary(
    const char* spmodel_buffer_data, size_t spmodel_buffer_size) {
  tokenizer_ = absl::make_unique<SentencePieceTokenizer>(spmodel_buffer_data,
                                                         spmodel_buffer_size);
}

}  // namespace text
}  // namespace task
}  // namespace tflite
