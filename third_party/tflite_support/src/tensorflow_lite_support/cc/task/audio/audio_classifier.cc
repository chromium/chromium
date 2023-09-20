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

#include "tensorflow_lite_support/cc/task/audio/audio_classifier.h"

#include <initializer_list>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow/lite/c/c_api_types.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/task/audio/proto/audio_classifier_options.pb.h"
#include "tensorflow_lite_support/cc/task/audio/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/audio/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/core/classification_head.h"
#include "tensorflow_lite_support/cc/task/core/label_map_item.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/processor/audio_preprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/classification_postprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/proto/classification_options.pb.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace audio {

namespace {

using ::absl::StatusCode;
using ::tflite::AudioProperties;
using ::tflite::ContentProperties;
using ::tflite::ContentProperties_AudioProperties;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::audio::Class;
using ::tflite::task::audio::ClassificationResult;
using ::tflite::task::core::AssertAndReturnTypedTensor;
using ::tflite::task::core::LabelMapItem;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

}  // namespace

StatusOr<std::unique_ptr<processor::ClassificationPostprocessor>>
CreatePostprocessor(TfLiteEngine* engine,
                    const std::initializer_list<int> output_indices,
                    AudioClassifierOptions* options) {
  auto new_options = std::make_unique<processor::ClassificationOptions>();
  new_options->set_display_names_locale(options->display_names_locale());
  new_options->set_max_results(options->max_results());
  new_options->set_score_threshold(options->score_threshold());
  new_options->mutable_class_name_allowlist()->Swap(
      options->mutable_class_name_allowlist());
  new_options->mutable_class_name_denylist()->Swap(
      options->mutable_class_name_denylist());
  return processor::ClassificationPostprocessor::Create(engine, output_indices,
                                                        std::move(new_options));
}

/* static */
StatusOr<std::unique_ptr<AudioClassifier>> AudioClassifier::CreateFromOptions(
    const AudioClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the constructed object.
  auto options_copy = absl::make_unique<AudioClassifierOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(auto audio_classifier,
                   TaskAPIFactory::CreateFromBaseOptions<AudioClassifier>(
                       &options_copy->base_options(), std::move(resolver)));

  TFLITE_RETURN_IF_ERROR(audio_classifier->Init(std::move(options_copy)));

  return audio_classifier;
}

/* static */
absl::Status AudioClassifier::SanityCheckOptions(
    const AudioClassifierOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

absl::Status AudioClassifier::Init(
    std::unique_ptr<AudioClassifierOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Create preprocessor, assuming having only 1 input tensor.
  TFLITE_ASSIGN_OR_RETURN(preprocessor_, processor::AudioPreprocessor::Create(
                                      GetTfLiteEngine(), {0}));

  // Assuming all output tensors share the same option. This is an limitation in
  // the current API design.
  int output_count =
      GetTfLiteEngine()->OutputCount(GetTfLiteEngine()->interpreter());
  postprocessors_.reserve(output_count);
  for (int i = 0; i < output_count; i++) {
    TFLITE_ASSIGN_OR_RETURN(auto processor, CreatePostprocessor(GetTfLiteEngine(), {i},
                                                         options_.get()));
    postprocessors_.emplace_back(std::move(processor));
  }

  return absl::OkStatus();
}

tflite::support::StatusOr<ClassificationResult> AudioClassifier::Classify(
    const AudioBuffer& audio_buffer) {
  return InferWithFallback(audio_buffer);
}

tflite::support::StatusOr<audio::ClassificationResult>
AudioClassifier::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const AudioBuffer& audio_buffer) {
  audio::ClassificationResult result;
  for (auto& processor : postprocessors_) {
    auto* classification = result.add_classifications();
    // ClassificationPostprocessor doesn't set head name for backward
    // compatibility, so we set it here manually.
    classification->set_head_name(processor->GetHeadName());
    TFLITE_RETURN_IF_ERROR(processor->Postprocess(classification));
  }

  return result;
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
