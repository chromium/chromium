/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/task/text/text_searcher.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/processor/bert_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/processor/regex_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/search_postprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/universal_sentence_encoder_preprocessor.h"
#include "tensorflow_lite_support/cc/task/text/proto/text_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/utils/bert_utils.h"
#include "tensorflow_lite_support/cc/task/text/utils/universal_sentence_encoder_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {

using ::tflite::support::StatusOr;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::processor::BertPreprocessor;
using ::tflite::task::processor::EmbeddingOptions;
using ::tflite::task::processor::RegexPreprocessor;
using ::tflite::task::processor::SearchOptions;
using ::tflite::task::processor::SearchPostprocessor;
using ::tflite::task::processor::SearchResult;
using ::tflite::task::processor::UniversalSentenceEncoderPreprocessor;

// Expected index of the response encoding output tensor in Universal Sentence
// Encoder models, as returned by GetUniversalSentenceEncoderOutputIndices().
constexpr int kUSEResponseEncodingIndex = 1;

}  // namespace

/* static */
StatusOr<std::unique_ptr<TextSearcher>> TextSearcher::CreateFromOptions(
    const TextSearcherOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  // Copy options to ensure the ExternalFile-s outlive the constructed object.
  auto options_copy = absl::make_unique<TextSearcherOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(auto text_searcher,
                   TaskAPIFactory::CreateFromBaseOptions<TextSearcher>(
                       &options_copy->base_options(), std::move(resolver)));
  TFLITE_RETURN_IF_ERROR(text_searcher->Init(std::move(options_copy)));

  return text_searcher;
}

absl::Status TextSearcher::Init(std::unique_ptr<TextSearcherOptions> options) {
  options_ = std::move(options);

  int input_count = GetInputCount();
  int output_count = GetOutputCount();
  int output_tensor_index;
  if (input_count == 1) {
    // Assume Regex-based model.
    if (output_count != 1) {
      return support::CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          absl::StrFormat("Expected exactly 1 output tensor, got %d.",
                          output_count));
    }
    TFLITE_ASSIGN_OR_RETURN(preprocessor_, processor::RegexPreprocessor::Create(
                                        GetTfLiteEngine(), 0));
    output_tensor_index = 0;
  } else if (input_count == 3) {
    // Check if BertTokenizer is present.
    if (GetMetadataExtractor()->GetInputProcessUnitsCount() > 0) {
      // Assume Bert-based model.
      if (output_count != 1) {
        return support::CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            absl::StrFormat("Expected exactly 1 output tensor, got %d.",
                            output_count));
      }
      TFLITE_ASSIGN_OR_RETURN(auto input_indices,
                       GetBertInputTensorIndices(GetTfLiteEngine()));
      TFLITE_ASSIGN_OR_RETURN(preprocessor_, processor::BertPreprocessor::Create(
                                          GetTfLiteEngine(),
                                          {input_indices[0], input_indices[1],
                                           input_indices[2]}));
      output_tensor_index = 0;
    } else {
      // Assume Universal Sentence Encoder-based model.
      TFLITE_ASSIGN_OR_RETURN(
          auto input_indices,
          GetUniversalSentenceEncoderInputTensorIndices(GetTfLiteEngine()));
      TFLITE_ASSIGN_OR_RETURN(
          auto output_indices,
          GetUniversalSentenceEncoderOutputTensorIndices(GetTfLiteEngine()));
      TFLITE_ASSIGN_OR_RETURN(
          preprocessor_,
          processor::UniversalSentenceEncoderPreprocessor::Create(
              GetTfLiteEngine(),
              {input_indices[0], input_indices[1], input_indices[2]}));
      // Only use the response encoding output.
      output_tensor_index = output_indices[kUSEResponseEncodingIndex];
    }
  } else {
    return support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected 1 or 3 input tensors, got %d.", input_count));
  }

  TFLITE_ASSIGN_OR_RETURN(
      postprocessor_,
      SearchPostprocessor::Create(
          GetTfLiteEngine(), output_tensor_index,
          std::make_unique<SearchOptions>(options_->search_options()),
          std::make_unique<EmbeddingOptions>(options_->embedding_options())));

  return absl::OkStatus();
}

StatusOr<SearchResult> TextSearcher::Search(const std::string& input) {
  return InferWithFallback(input);
}

StatusOr<absl::string_view> TextSearcher::GetUserInfo() {
  return postprocessor_->GetUserInfo();
}

absl::Status TextSearcher::Preprocess(
    const std::vector<TfLiteTensor*>& /*input_tensors*/,
    const std::string& input) {
  return preprocessor_->Preprocess(input);
}

StatusOr<SearchResult> TextSearcher::Postprocess(
    const std::vector<const TfLiteTensor*>& /*output_tensors*/,
    const std::string& /*input*/) {
  return postprocessor_->Postprocess();
}

}  // namespace text
}  // namespace task
}  // namespace tflite
