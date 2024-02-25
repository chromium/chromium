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
#include "tensorflow_lite_support/cc/task/audio/audio_embedder.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/audio/proto/audio_embedder_options.pb.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/processor/audio_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/embedding_postprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"

namespace tflite {
namespace task {
namespace audio {

/* static */
tflite::support::StatusOr<double> AudioEmbedder::CosineSimilarity(
    const processor::FeatureVector& u, const processor::FeatureVector& v) {
  return processor::EmbeddingPostprocessor::CosineSimilarity(u, v);
}

/* static */
tflite::support::StatusOr<std::unique_ptr<AudioEmbedder>>
AudioEmbedder::CreateFromOptions(const AudioEmbedderOptions& options,
                                 std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));
  auto options_copy = absl::make_unique<AudioEmbedderOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(auto audio_embedder,
                   core::TaskAPIFactory::CreateFromBaseOptions<AudioEmbedder>(
                       &options_copy->base_options(), std::move(resolver)));

  TFLITE_RETURN_IF_ERROR(audio_embedder->Init(std::move(options_copy)));
  return audio_embedder;
}

/* static */
absl::Status AudioEmbedder::SanityCheckOptions(
    const AudioEmbedderOptions& options) {
  if (!options.has_base_options()) {
    return support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Missing mandatory `base_options` field",
        support::TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

absl::Status AudioEmbedder::Init(
    std::unique_ptr<AudioEmbedderOptions> options) {
  options_ = std::move(options);

  // Create preprocessor, assuming having only 1 input tensor.
  TFLITE_ASSIGN_OR_RETURN(preprocessor_,
                   tflite::task::processor::AudioPreprocessor::Create(
                       GetTfLiteEngine(), {0}));

  // Create postprocessors, assuming that all output tensors are embedding
  // outputs.
  int post_processors_count =
      GetTfLiteEngine()->OutputCount(GetTfLiteEngine()->interpreter());
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
    TFLITE_ASSIGN_OR_RETURN(auto processor,
                     processor::EmbeddingPostprocessor::Create(
                         GetTfLiteEngine(), {i}, std::move(option)));
    postprocessors_.emplace_back(std::move(processor));
  }
  return absl::OkStatus();
}

tflite::support::StatusOr<tflite::task::processor::EmbeddingResult>
AudioEmbedder::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const AudioBuffer& audio_buffer) {
  tflite::task::processor::EmbeddingResult result;
  for (int i = 0; i < postprocessors_.size(); i++) {
    auto processor = postprocessors_.at(i).get();
    TFLITE_RETURN_IF_ERROR(processor->Postprocess(result.add_embeddings()));
  }
  return result;
}

tflite::support::StatusOr<tflite::task::processor::EmbeddingResult>
AudioEmbedder::Embed(const AudioBuffer& audio_buffer) {
  return InferWithFallback(audio_buffer);
}

int AudioEmbedder::GetEmbeddingDimension(int output_index) const {
  if (output_index < 0 || output_index >= postprocessors_.size()) {
    return -1;
  }
  return postprocessors_.at(output_index)->GetEmbeddingDimension();
}

int AudioEmbedder::GetNumberOfOutputLayers() const {
  return postprocessors_.size();
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
