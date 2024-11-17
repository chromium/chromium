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

#include "tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/category.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/cc/task/text/utils/bert_utils.h"

namespace tflite {
namespace task {
namespace text {

using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::FindTensorByName;

namespace {
constexpr const char* kValidScoreTensorNames[] = { "probability", "score" };

absl::Status SanityCheckOptions(const BertNLClassifierOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(absl::StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status BertNLClassifier::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors, const std::string& input) {
  return preprocessor_->Preprocess(input);
}

StatusOr<std::vector<core::Category>> BertNLClassifier::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::string& /*input*/) {
  if (output_tensors.size() != 1) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("BertNLClassifier models are expected to have only 1 "
                        "output, found %d",
                        output_tensors.size()),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }
  for (const auto& name : kValidScoreTensorNames) {
    const TfLiteTensor* scores = FindTensorByName(
      output_tensors, GetMetadataExtractor()->GetOutputTensorMetadata(),
      name);
    if (scores) {
      // optional labels extracted from metadata
      return BuildResults(scores, /*labels=*/nullptr);
    }
  }
  return CreateStatusWithPayload(
      absl::StatusCode::kInvalidArgument,
      absl::StrFormat("BertNLClassifier models are expected to have an output "
                      "tensor by the name: 'score' or 'probability'"),
      TfLiteSupportStatus::kOutputTensorNotFoundError);
}

StatusOr<std::unique_ptr<BertNLClassifier>> BertNLClassifier::CreateFromOptions(
    const BertNLClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  auto options_copy = absl::make_unique<BertNLClassifierOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(
      auto bert_nl_classifier,
      core::TaskAPIFactory::CreateFromBaseOptions<BertNLClassifier>(
          &options_copy->base_options(), std::move(resolver)));
  TFLITE_RETURN_IF_ERROR(bert_nl_classifier->Initialize(std::move(options_copy)));
  return std::move(bert_nl_classifier);
}

absl::Status BertNLClassifier::Initialize(
    std::unique_ptr<BertNLClassifierOptions> options) {
  options_ = std::move(options);

  // Create preprocessor.
  TFLITE_ASSIGN_OR_RETURN(auto input_indices,
                   GetBertInputTensorIndices(GetTfLiteEngine()));
  TFLITE_ASSIGN_OR_RETURN(preprocessor_,
                   processor::BertPreprocessor::Create(
                       GetTfLiteEngine(),
                       {input_indices[0], input_indices[1], input_indices[2]}));

  // Set up optional label vector from metadata.
  TrySetLabelFromMetadata(
      GetMetadataExtractor()->GetOutputTensorMetadata(kOutputTensorIndex))
      .IgnoreError();

  return absl::OkStatus();
}

}  // namespace text
}  // namespace task
}  // namespace tflite
