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
#include "tensorflow_lite_support/cc/task/processor/processor.h"

#include <iterator>
#include <sstream>

namespace tflite {
namespace task {
namespace processor {

constexpr char Preprocessor::kInputTypeName[];
constexpr char Postprocessor::kOutputTypeName[];

absl::Status Processor::SanityCheck(int num_expected_tensors,
                                    bool requires_metadata) {
  const char* tensor_type_name = GetTensorTypeName();
  if (tensor_indices_.size() != num_expected_tensors) {
    return support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Processor can handle %d tensors, "
                        "got: %d tensors.",
                        num_expected_tensors, tensor_indices_.size()));
  }

  int tensor_count = GetModelTensorCount();
  for (int i = 0; i < tensor_indices_.size(); i++) {
    int index = tensor_indices_.at(i);
    if (index < 0 || index >= tensor_count) {
      return support::CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          absl::StrFormat("Invalid tensor_index: %d. Model has %d %s tensors.",
                          index, tensor_count, tensor_type_name));
    }
    if (requires_metadata) {
      if (GetTensorMetadata(i) == nullptr) {
        return CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            absl::StrFormat("%s tensor %d is missing TensorMetadata.",
                            tensor_type_name, index),
            support::TfLiteSupportStatus::kMetadataNotFoundError);
      }
    }
  }

  return absl::OkStatus();
}

std::string Processor::GetTensorIndexString() {
  std::stringstream stream;
  std::copy(tensor_indices_.begin(), tensor_indices_.end(),
            std::ostream_iterator<int>(stream, " "));
  return stream.str();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
