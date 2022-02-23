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

#include "tensorflow_lite_support/cc/task/text/text_embedder.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/processor/bert_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/regex_preprocessor.h"

namespace tflite {
namespace task {
namespace text {

namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::processor::EmbeddingResult;
using ::tflite::task::processor::FeatureVector;

absl::Status SanityCheckOptions(const TextEmbedderOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

}  // namespace

/* static */
tflite::support::StatusOr<double> TextEmbedder::CosineSimilarity(
    const FeatureVector& u,
    const FeatureVector& v) {
  return processor::EmbeddingPostprocessor::CosineSimilarity(u, v);
}

/* static */
tflite::support::StatusOr<std::unique_ptr<TextEmbedder>>
TextEmbedder::CreateFromOptions(const TextEmbedderOptions& options,
                                std::unique_ptr<tflite::OpResolver> resolver) {
  RETURN_IF_ERROR(SanityCheckOptions(options));
  // Copy options to ensure the ExternalFile-s outlive the constructed object.
  auto options_copy = absl::make_unique<TextEmbedderOptions>(options);

  ASSIGN_OR_RETURN(auto text_embedder,
                   TaskAPIFactory::CreateFromBaseOptions<TextEmbedder>(
                       &options_copy->base_options(), std::move(resolver)));

  RETURN_IF_ERROR(text_embedder->Init(std::move(options_copy)));

  return text_embedder;
}

absl::Status TextEmbedder::Init(std::unique_ptr<TextEmbedderOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Assuming have only 1 input tensor for RegexPreprocessor and 3 input tensors
  // for BertPreprocessor.

  int32_t input_count = GetInputCount();
  if (input_count == 1) {
    ASSIGN_OR_RETURN(preprocessor_, processor::RegexPreprocessor::Create(
                                        GetTfLiteEngine(), 0));
  } else if (input_count == 3) {
    ASSIGN_OR_RETURN(preprocessor_, processor::BertPreprocessor::Create(
                                        GetTfLiteEngine(), {0, 1, 2}));
  } else {
    return support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Processor can handle 1 tensor or 3 tensors, "
                        "got: %d tensors.",
                        input_count));
  }

  // Create postprocessors, assuming that all output tensors are embedding
  // outputs.
  int post_processors_count =
      GetTfLiteEngine()->interpreter()->outputs().size();
  postprocessors_.reserve(post_processors_count);

  for (int i = 0; i < post_processors_count; i++) {
    std::unique_ptr<processor::EmbeddingOptions> option = nullptr;
    if (options_->embedding_options_size() == 0) {
      // Default options.
      option = std::make_unique<processor::EmbeddingOptions>();
    } else if (options_->embedding_options_size() == 1) {
      // Share the first options.
      option = std::make_unique<processor::EmbeddingOptions>(
          options_->embedding_options(0));
    } else if (options_->embedding_options_size() == post_processors_count) {
      option = std::make_unique<processor::EmbeddingOptions>(
          // Use the corresponding options for the tensor.
          options_->embedding_options(i));
    } else {
      return support::CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "Invalid embedding_options. It should have size of either 0, 1 or "
          "number of output tensors.",
          support::TfLiteSupportStatus::kInvalidArgumentError);
    }
    ASSIGN_OR_RETURN(auto processor,
                     processor::EmbeddingPostprocessor::Create(
                         GetTfLiteEngine(), {i}, std::move(option)));
    postprocessors_.emplace_back(std::move(processor));
  }

  return absl::OkStatus();
}

tflite::support::StatusOr<EmbeddingResult> TextEmbedder::Embed(
    const std::string& text) {
  return InferWithFallback(text);
}

absl::Status TextEmbedder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::string& input) {
  return preprocessor_->Preprocess(input);
}

// TODO(b/215239176): Creates base Embedder class to share Postprocess /
// GetEmbeddingDimension / GetNumberOfOutputLayers / CosineSimilarity /
// Init (how it creates postprocessors) funcrions between
// Image/Text/AudioEmbedder.
tflite::support::StatusOr<EmbeddingResult> TextEmbedder::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::string& input) {
  EmbeddingResult result;
  for (int i = 0; i < postprocessors_.size(); ++i) {
    RETURN_IF_ERROR(
        postprocessors_.at(i)->Postprocess(result.add_embeddings()));
  }

  return result;
}

int TextEmbedder::GetEmbeddingDimension(int output_index) const {
  if (output_index < 0 || output_index >= postprocessors_.size()) {
    return -1;
  }
  return postprocessors_.at(output_index)->GetEmbeddingDimension();
}

int TextEmbedder::GetNumberOfOutputLayers() const {
  return postprocessors_.size();
}

}  // namespace text
}  // namespace task
}  // namespace tflite
