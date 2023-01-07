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

#include "tensorflow_lite_support/cc/task/text/utils/universal_sentence_encoder_utils.h"

#include <algorithm>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace tflite {
namespace task {
namespace text {

namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::task::core::FindTensorIndexByName;
using ::tflite::task::core::TfLiteEngine;

constexpr char kQueryTextMetadataName[] = "inp_text";
constexpr char kResponseContextMetadataName[] = "res_context";
constexpr char kResponseTextMetadataName[] = "res_text";
constexpr char kQueryEncodingMetadataName[] = "query_encoding";
constexpr char kResponseEncodingMetadataName[] = "response_encoding";

constexpr char kQueryTextTensorName[] = "ParseExample/ParseExampleV2:1";
constexpr char kResponseContextTensorName[] = "ParseExample/ParseExampleV2:2";
constexpr char kResponseTextTensorName[] = "ParseExample/ParseExampleV2:3";
constexpr char kQueryEncodingTensorName[] = "Final/EncodeQuery/mul";
constexpr char kResponseEncodingTensorName[] = "Final/EncodeResult/mul";

constexpr int kDefaultInputTensorIndices[3] = {0, 1, 2};
constexpr int kDefaultOutputTensorIndices[2] = {0, 1};

}  // namespace

StatusOr<std::vector<int>> GetUniversalSentenceEncoderInputTensorIndices(
    TfLiteEngine* engine) {
  auto input_tensors = engine->GetInputs();
  if (input_tensors.size() < 3) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected at least 3 input tensors (query text, "
                        "response context, response text), found %d.",
                        input_tensors.size()));
  }
  auto* input_tensors_metadata =
      engine->metadata_extractor()->GetInputTensorMetadata();
  std::vector<int> input_indices = {
      FindTensorIndexByName(input_tensors, input_tensors_metadata,
                            kQueryTextMetadataName, kQueryTextTensorName),
      FindTensorIndexByName(input_tensors, input_tensors_metadata,
                            kResponseContextMetadataName,
                            kResponseContextTensorName),
      FindTensorIndexByName(input_tensors, input_tensors_metadata,
                            kResponseTextMetadataName,
                            kResponseTextTensorName)};
  if (std::find(input_indices.begin(), input_indices.end(), -1) !=
      input_indices.end()) {
    // Use the default indices if any input tensor is not found.
    input_indices = std::vector<int>(std::begin(kDefaultInputTensorIndices),
                                     std::end(kDefaultInputTensorIndices));
  }
  return input_indices;
}

StatusOr<std::vector<int>> GetUniversalSentenceEncoderOutputTensorIndices(
    TfLiteEngine* engine) {
  auto output_tensors = engine->GetOutputs();
  if (output_tensors.size() < 2) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected at least 2 output tensors (query encoding, "
                        "response encoding), found %d.",
                        output_tensors.size()));
  }
  auto* output_tensors_metadata =
      engine->metadata_extractor()->GetOutputTensorMetadata();
  std::vector<int> output_indices = {
      FindTensorIndexByName(output_tensors, output_tensors_metadata,
                            kQueryEncodingMetadataName,
                            kQueryEncodingTensorName),
      FindTensorIndexByName(output_tensors, output_tensors_metadata,
                            kResponseEncodingMetadataName,
                            kResponseEncodingTensorName)};

  if (std::find(output_indices.begin(), output_indices.end(), -1) !=
      output_indices.end()) {
    // Use the default indices if any output tensor is not found.
    output_indices = std::vector<int>(std::begin(kDefaultOutputTensorIndices),
                                      std::end(kDefaultOutputTensorIndices));
  }
  return output_indices;
}

}  // namespace text
}  // namespace task
}  // namespace tflite
