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

#include "tensorflow_lite_support/cc/task/text/utils/bert_utils.h"

#include <algorithm>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::task::core::FindTensorIndexByMetadataName;
using ::tflite::task::core::TfLiteEngine;

constexpr char kIdsTensorName[] = "ids";
constexpr char kSegmentIdsTensorName[] = "segment_ids";
constexpr char kMaskTensorName[] = "mask";

constexpr int kDefaultInputTensorIndices[3] = {0, 1, 2};

}  // namespace

StatusOr<std::vector<int>> GetBertInputTensorIndices(TfLiteEngine* engine) {
  auto input_tensors = engine->GetInputs();
  if (input_tensors.size() < 3) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected at least 3 input tensors (ids, mask, segment "
                        "ids), found %d.",
                        input_tensors.size()));
  }
  auto* input_tensors_metadata =
      engine->metadata_extractor()->GetInputTensorMetadata();
  std::vector<int> input_indices = {
      FindTensorIndexByMetadataName(input_tensors_metadata, kIdsTensorName),
      FindTensorIndexByMetadataName(input_tensors_metadata,
                                    kSegmentIdsTensorName),
      FindTensorIndexByMetadataName(input_tensors_metadata, kMaskTensorName)};
  if (std::find(input_indices.begin(), input_indices.end(), -1) !=
      input_indices.end()) {
    // Use the default indices if any input tensor is not found.
    input_indices = std::vector<int>(std::begin(kDefaultInputTensorIndices),
                                     std::end(kDefaultInputTensorIndices));
  }
  return input_indices;
}

}  // namespace text
}  // namespace task
}  // namespace tflite
