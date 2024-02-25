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

#include "tensorflow_lite_support/cc/task/vision/image_embedder.h"

#include <algorithm>

#include "absl/container/node_hash_set.h"  // from @com_google_absl
#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
using ::tflite::task::core::TaskAPIFactory;

tflite::support::StatusOr<std::unique_ptr<processor::EmbeddingPostprocessor>>
CreatePostprocessor(core::TfLiteEngine* engine,
                    const std::initializer_list<int> output_indices,
                    const ImageEmbedderOptions& options) {
  auto new_options = std::make_unique<processor::EmbeddingOptions>();
  new_options->set_l2_normalize(options.l2_normalize());
  new_options->set_quantize(options.quantize());
  return processor::EmbeddingPostprocessor::Create(engine, output_indices,
                                                   std::move(new_options));
}
}  // namespace

/* static */
tflite::support::StatusOr<double> ImageEmbedder::CosineSimilarity(
    const FeatureVector& u, const FeatureVector& v) {
  return processor::EmbeddingPostprocessor::CosineSimilarity(u, v);
}

/* static */
tflite::support::StatusOr<std::unique_ptr<ImageEmbedder>>
ImageEmbedder::CreateFromOptions(const ImageEmbedderOptions& options,
                                 std::unique_ptr<tflite::OpResolver> resolver) {
  // Copy options to ensure the ExternalFile-s outlive the constructed object.
  auto options_copy = absl::make_unique<ImageEmbedderOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(
      auto image_embedder,
      TaskAPIFactory::CreateFromExternalFileProto<ImageEmbedder>(
          &options_copy->model_file_with_metadata(), std::move(resolver),
          options_copy->num_threads(), options_copy->compute_settings()));

  TFLITE_RETURN_IF_ERROR(image_embedder->Init(std::move(options_copy)));

  return image_embedder;
}

absl::Status ImageEmbedder::PreInit() {
  SetProcessEngine(FrameBufferUtils::ProcessEngine::kLibyuv);
  return absl::OkStatus();
}

absl::Status ImageEmbedder::PostInit() {
  // Nothing to do.
  return absl::OkStatus();
}

absl::Status ImageEmbedder::Init(
    std::unique_ptr<ImageEmbedderOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Perform pre-initialization actions.
  TFLITE_RETURN_IF_ERROR(PreInit());

  // Sanity check and set inputs and outputs.
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());

  // Perform post-initialization actions.
  TFLITE_RETURN_IF_ERROR(PostInit());

  // ImageEmbedder assumes that all output tensors share the same
  // embedding option.
  postprocessors_.reserve(GetTfLiteEngine()->interpreter()->outputs().size());
  for (int i = 0; i < GetTfLiteEngine()->interpreter()->outputs().size(); i++) {
    TFLITE_ASSIGN_OR_RETURN(auto processor,
                     CreatePostprocessor(GetTfLiteEngine(), {i}, *options_));
    postprocessors_.emplace_back(std::move(processor));
  }

  return absl::OkStatus();
}

tflite::support::StatusOr<EmbeddingResult> ImageEmbedder::Embed(
    const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return Embed(frame_buffer, roi);
}

tflite::support::StatusOr<EmbeddingResult> ImageEmbedder::Embed(
    const FrameBuffer& frame_buffer, const BoundingBox& roi) {
  return InferWithFallback(frame_buffer, roi);
}

tflite::support::StatusOr<EmbeddingResult> ImageEmbedder::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const FrameBuffer& /*frame_buffer*/, const BoundingBox& /*roi*/) {
  EmbeddingResult result;
  for (int i = 0; i < postprocessors_.size(); ++i) {
    TFLITE_RETURN_IF_ERROR(
        postprocessors_.at(i)->Postprocess(result.add_embeddings()));
  }

  return result;
}

Embedding ImageEmbedder::GetEmbeddingByIndex(const EmbeddingResult& result,
                                             int output_index) {
  if (output_index < 0 || output_index >= postprocessors_.size()) {
    return Embedding();
  }
  return result.embeddings(output_index);
}

int ImageEmbedder::GetEmbeddingDimension(int output_index) const {
  if (output_index < 0 || output_index >= postprocessors_.size()) {
    return -1;
  }
  return postprocessors_.at(output_index)->GetEmbeddingDimension();
}

int ImageEmbedder::GetNumberOfOutputLayers() const {
  return postprocessors_.size();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
