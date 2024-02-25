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

#include "tensorflow_lite_support/cc/task/processor/embedding_postprocessor.h"

namespace tflite {
namespace task {
namespace processor {

/* static */
tflite::support::StatusOr<std::unique_ptr<EmbeddingPostprocessor>>
EmbeddingPostprocessor::Create(core::TfLiteEngine* engine,
                               const std::initializer_list<int> output_indices,
                               std::unique_ptr<EmbeddingOptions> options) {
  TFLITE_ASSIGN_OR_RETURN(auto processor,
                   Processor::Create<EmbeddingPostprocessor>(
                       /* num_expected_tensors = */ 1, engine, output_indices,
                       /* requires_metadata = */ false));

  TFLITE_RETURN_IF_ERROR(processor->Init(std::move(options)));
  return processor;
}

absl::Status EmbeddingPostprocessor::Init(
    std::unique_ptr<EmbeddingOptions> options) {
  options_ = std::move(options);

  int output_index = tensor_indices_.at(0);
  auto* output_tensor = GetTensor();
  int num_dimensions = output_tensor->dims->size;
  if (num_dimensions == 4) {
    if (output_tensor->dims->data[1] != 1 ||
        output_tensor->dims->data[2] != 1) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          absl::StrFormat("Unexpected WxH sizes for output index %d: got "
                          "%dx%d, expected 1x1.",
                          output_index, output_tensor->dims->data[2],
                          output_tensor->dims->data[1]),
          support::TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
    }
  } else if (num_dimensions != 2) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Unexpected number of dimensions for output index %d: got %dD, "
            "expected either 2D (BxN with B=1) or 4D (BxHxWxN with B=1, "
            "W=1, "
            "H=1).",
            output_index, num_dimensions),
        support::TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
  }
  if (output_tensor->dims->data[0] != 1) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("The output array is expected to have a batch size "
                        "of 1. Got %d for output index %d.",
                        output_tensor->dims->data[0], output_index),
        support::TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
  }
  embedding_dimension_ = output_tensor->dims->data[num_dimensions - 1];
  if (output_tensor->type != kTfLiteUInt8 &&
      output_tensor->type != kTfLiteFloat32) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Type mismatch for output tensor %s. Requested one "
                        "of these types: "
                        "kTfLiteUint8/kTfLiteFloat32, got %s.",
                        output_tensor->name,
                        TfLiteTypeGetName(output_tensor->type)),
        support::TfLiteSupportStatus::kInvalidOutputTensorTypeError);
  }
  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
