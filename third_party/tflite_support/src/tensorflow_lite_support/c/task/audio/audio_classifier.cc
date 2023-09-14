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

#include "tensorflow_lite_support/c/task/audio/audio_classifier.h"

#include <memory>

#include "tensorflow_lite_support/c/common_utils.h"
#include "tensorflow_lite_support/c/task/audio/utils/audio_buffer_cpp_c_utils.h"
#include "tensorflow_lite_support/c/task/core/utils/base_options_utils.h"
#include "tensorflow_lite_support/c/task/processor/utils/classification_options_utils.h"
#include "tensorflow_lite_support/cc/task/audio/audio_classifier.h"
#include "tensorflow_lite_support/cc/task/audio/proto/audio_classifier_options.pb.h"
#include "tensorflow_lite_support/cc/task/audio/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/audio/proto/classifications_proto_inc.h"

namespace {
using ::tflite::support::StatusOr;
using ClassificationResultCpp = ::tflite::task::audio::ClassificationResult;
using ClassificationsCpp = ::tflite::task::audio::Classifications;
using ClassCpp = ::tflite::task::audio::Class;
using AudioClassifierCpp = ::tflite::task::audio::AudioClassifier;
using AudioClassifierOptionsCpp = ::tflite::task::audio::AudioClassifierOptions;
using AudioBufferCpp = ::tflite::task::audio::AudioBuffer;
using ::tflite::support::TfLiteSupportStatus;

StatusOr<AudioClassifierOptionsCpp> CreateAudioClassifierCppOptionsFromCOptions(
    const TfLiteAudioClassifierOptions* c_options) {
  if (c_options == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null options."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  AudioClassifierOptionsCpp cpp_options = {};

  // More file sources can be added in else ifs
  if (c_options->base_options.model_file.file_path)
    cpp_options.mutable_base_options()->mutable_model_file()->set_file_name(
        c_options->base_options.model_file.file_path);

  // c_options->base_options.compute_settings.num_threads is expected to be
  // set to value > 0 or -1. Otherwise invoking
  // ImageClassifierCpp::CreateFromOptions() results in a not ok status.
  cpp_options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(
          c_options->base_options.compute_settings.cpu_settings.num_threads);

  for (int i = 0; i < c_options->classification_options.label_denylist.length;
       i++)
    cpp_options.add_class_name_denylist(
        c_options->classification_options.label_denylist.list[i]);

  for (int i = 0; i < c_options->classification_options.label_allowlist.length;
       i++)
    cpp_options.add_class_name_allowlist(
        c_options->classification_options.label_allowlist.list[i]);

  // Check needed since setting a nullptr for this field results in a segfault
  // on invocation of ImageClassifierCpp::CreateFromOptions().
  if (c_options->classification_options.display_names_local) {
    cpp_options.set_display_names_locale(
        c_options->classification_options.display_names_local);
  }

  // c_options->classification_options.max_results is expected to be set to -1
  // or any value > 0. Otherwise invoking
  // ImageClassifierCpp::CreateFromOptions() results in a not ok status.
  cpp_options.set_max_results(c_options->classification_options.max_results);

  cpp_options.set_score_threshold(
      c_options->classification_options.score_threshold);

  return cpp_options;
}
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct TfLiteAudioClassifier {
  std::unique_ptr<AudioClassifierCpp> impl;
};

TfLiteAudioClassifierOptions TfLiteAudioClassifierOptionsCreate(void) {
  // Use brace-enclosed initializer list will break the Kokoro test.
  TfLiteAudioClassifierOptions options;
  options.classification_options =
      tflite::task::processor::CreateDefaultClassificationOptions();
  options.base_options = tflite::task::core::CreateDefaultBaseOptions();
  return options;
}

TfLiteAudioClassifier* TfLiteAudioClassifierFromOptions(
    const TfLiteAudioClassifierOptions* options, TfLiteSupportError** error) {
  StatusOr<AudioClassifierOptionsCpp> cpp_option_status =
      CreateAudioClassifierCppOptionsFromCOptions(options);

  if (!cpp_option_status.ok()) {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_option_status.status(), error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<AudioClassifierCpp>> classifier_status =
      AudioClassifierCpp::CreateFromOptions(cpp_option_status.value());

  if (classifier_status.ok()) {
    return new TfLiteAudioClassifier{.impl =
                                         std::move(classifier_status.value())};
  } else {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        classifier_status.status(), error);
    return nullptr;
  }
}

TfLiteClassificationResult* GetClassificationResultCStruct(
    const ClassificationResultCpp& classification_result_cpp) {
  auto c_classifications =
      new TfLiteClassifications[classification_result_cpp
                                    .classifications_size()];

  for (int head = 0; head < classification_result_cpp.classifications_size();
       ++head) {
    const ClassificationsCpp& classifications =
        classification_result_cpp.classifications(head);
    c_classifications[head].head_index = classifications.head_index();

    if (classifications.has_head_name()) {
      c_classifications[head].head_name =
          strdup(classifications.head_name().c_str());
    }

    auto c_categories = new TfLiteCategory[classifications.classes_size()];
    c_classifications->size = classifications.classes_size();

    for (int rank = 0; rank < classifications.classes_size(); ++rank) {
      const ClassCpp& classification = classifications.classes(rank);
      c_categories[rank].index = classification.index();
      c_categories[rank].score = classification.score();

      if (classification.has_class_name())
        c_categories[rank].label = strdup(classification.class_name().c_str());
      else
        c_categories[rank].label = nullptr;

      if (classification.has_display_name())
        c_categories[rank].display_name =
            strdup(classification.display_name().c_str());
      else
        c_categories[rank].display_name = nullptr;
    }
    c_classifications[head].categories = c_categories;
  }

  auto c_classification_result = new TfLiteClassificationResult;
  c_classification_result->classifications = c_classifications;
  c_classification_result->size =
      classification_result_cpp.classifications_size();

  return c_classification_result;
}

TfLiteClassificationResult* TfLiteAudioClassifierClassify(
    const TfLiteAudioClassifier* classifier,
    const TfLiteAudioBuffer* audio_buffer, TfLiteSupportError** error) {
  if (classifier == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null audio classifier.", error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<AudioBufferCpp>> cpp_audio_buffer_status =
      ::tflite::task::audio::CreateCppAudioBuffer(audio_buffer);
  if (!cpp_audio_buffer_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_audio_buffer_status.status(), error);
    return nullptr;
  }

  // fnc_sample(cpp_audio_buffer_status);
  StatusOr<ClassificationResultCpp> cpp_classification_result_status =
      classifier->impl->Classify(*(cpp_audio_buffer_status.value()));

  if (!cpp_classification_result_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_classification_result_status.status(), error);
    return nullptr;
  }

  return GetClassificationResultCStruct(
      cpp_classification_result_status.value());
}

int TfLiteAudioClassifierGetRequiredInputBufferSize(
    TfLiteAudioClassifier* classifier, TfLiteSupportError** error) {
  if (classifier == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null audio classifier.", error);
    return -1;
  }

  return classifier->impl->GetRequiredInputBufferSize();
}

void TfLiteAudioClassifierDelete(TfLiteAudioClassifier* classifier) {
  delete classifier;
}

TfLiteAudioFormat* TfLiteAudioClassifierGetRequiredAudioFormat(
    TfLiteAudioClassifier* classifier, TfLiteSupportError** error) {
  if (classifier == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null audio classifier.", error);
    return nullptr;
  }

  StatusOr<TfLiteAudioFormat*> c_audio_format =
      CreateCAudioFormat(classifier->impl->GetRequiredAudioFormat());

  if (!c_audio_format.ok()) {
    return nullptr;
  }

  return c_audio_format.value();
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
