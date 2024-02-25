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

#include "tensorflow_lite_support/cc/task/vision/image_searcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/processor/search_postprocessor.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {

using ::tflite::support::StatusOr;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::processor::SearchPostprocessor;
using ::tflite::task::processor::SearchResult;

}  // namespace

/* static */
StatusOr<std::unique_ptr<ImageSearcher>> ImageSearcher::CreateFromOptions(
    const ImageSearcherOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  // Copy options to ensure the ExternalFile-s outlive the constructed object.
  auto options_copy = absl::make_unique<ImageSearcherOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(auto image_searcher,
                   TaskAPIFactory::CreateFromBaseOptions<ImageSearcher>(
                       &options_copy->base_options(), std::move(resolver)));

  TFLITE_RETURN_IF_ERROR(image_searcher->Init(std::move(options_copy)));

  return image_searcher;
}

absl::Status ImageSearcher::PreInit() {
  SetProcessEngine(FrameBufferUtils::ProcessEngine::kLibyuv);
  return absl::OkStatus();
}

absl::Status ImageSearcher::Init(
    std::unique_ptr<ImageSearcherOptions> options) {
  options_ = std::move(options);

  // Perform pre-initialization actions.
  TFLITE_RETURN_IF_ERROR(PreInit());

  // Sanity check and set inputs.
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());

  // Create post-processor.
  TFLITE_ASSIGN_OR_RETURN(
      postprocessor_,
      SearchPostprocessor::Create(GetTfLiteEngine(), 0,
                                  std::make_unique<processor::SearchOptions>(
                                      options_->search_options()),
                                  std::make_unique<processor::EmbeddingOptions>(
                                      options_->embedding_options())));

  return absl::OkStatus();
}

StatusOr<SearchResult> ImageSearcher::Search(const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return Search(frame_buffer, roi);
}

StatusOr<SearchResult> ImageSearcher::Search(const FrameBuffer& frame_buffer,
                                             const BoundingBox& roi) {
  return InferWithFallback(frame_buffer, roi);
}

StatusOr<absl::string_view> ImageSearcher::GetUserInfo() {
  return postprocessor_->GetUserInfo();
}

StatusOr<SearchResult> ImageSearcher::Postprocess(
    const std::vector<const TfLiteTensor*>& /*output_tensors*/,
    const FrameBuffer& /*frame_buffer*/, const BoundingBox& /*roi*/) {
  return postprocessor_->Postprocess();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
