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

#include "tensorflow_lite_support/c/task/vision/image_classifier.h"

#include <memory>

#include "tensorflow_lite_support/c/common_utils.h"
#include "tensorflow_lite_support/c/task/core/utils/base_options_utils.h"
#include "tensorflow_lite_support/c/task/processor/utils/classification_options_utils.h"
#include "tensorflow_lite_support/c/task/vision/utils/frame_buffer_cpp_c_utils.h"
#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "tensorflow_lite_support/cc/task/vision/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_classifier_options_proto_inc.h"

namespace {
using ::tflite::support::StatusOr;
using ClassificationResultCpp = ::tflite::task::vision::ClassificationResult;
using ClassificationsCpp = ::tflite::task::vision::Classifications;
using ClassCpp = ::tflite::task::vision::Class;
using BoundingBoxCpp = ::tflite::task::vision::BoundingBox;
using ImageClassifierCpp = ::tflite::task::vision::ImageClassifier;
using ImageClassifierOptionsCpp =
    ::tflite::task::vision::ImageClassifierOptions;
using FrameBufferCpp = ::tflite::task::vision::FrameBuffer;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::TfLiteSettingsProtoFromCOptions;

StatusOr<ImageClassifierOptionsCpp> CreateImageClassifierCppOptionsFromCOptions(
    const TfLiteImageClassifierOptions* c_options) {
  if (c_options == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null options."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  ImageClassifierOptionsCpp cpp_options = {};

  // More file sources can be added in else ifs
  if (c_options->base_options.model_file.file_path)
    cpp_options.mutable_base_options()->mutable_model_file()->set_file_name(
        c_options->base_options.model_file.file_path);

  // Sets the generic TfLiteSettings (CPU, Core ML Delegate, etc.) proto.
  *(cpp_options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()) =
      TfLiteSettingsProtoFromCOptions(
          &c_options->base_options.compute_settings);

  for (int i = 0; i < c_options->classification_options.label_denylist.length;
       i++)
    cpp_options.add_class_name_blacklist(
        c_options->classification_options.label_denylist.list[i]);

  for (int i = 0; i < c_options->classification_options.label_allowlist.length;
       i++)
    cpp_options.add_class_name_whitelist(
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

struct TfLiteImageClassifier {
  std::unique_ptr<ImageClassifierCpp> impl;
};

TfLiteImageClassifierOptions TfLiteImageClassifierOptionsCreate(void) {
  // Use brace-enclosed initializer list will break the Kokoro test.
  TfLiteImageClassifierOptions options;
  options.classification_options =
      tflite::task::processor::CreateDefaultClassificationOptions();
  options.base_options = tflite::task::core::CreateDefaultBaseOptions();
  return options;
}

TfLiteImageClassifier* TfLiteImageClassifierFromOptions(
    const TfLiteImageClassifierOptions* options, TfLiteSupportError** error) {
  StatusOr<ImageClassifierOptionsCpp> cpp_option_status =
      CreateImageClassifierCppOptionsFromCOptions(options);

  if (!cpp_option_status.ok()) {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_option_status.status(), error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<ImageClassifierCpp>> classifier_status =
      ImageClassifierCpp::CreateFromOptions(cpp_option_status.value());

  if (classifier_status.ok()) {
    return new TfLiteImageClassifier{.impl =
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
    c_classifications[head].head_name = nullptr;

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

TfLiteClassificationResult* TfLiteImageClassifierClassifyWithRoi(
    const TfLiteImageClassifier* classifier,
    const TfLiteFrameBuffer* frame_buffer, const TfLiteBoundingBox* roi,
    TfLiteSupportError** error) {
  if (classifier == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null image classifier.", error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<FrameBufferCpp>> cpp_frame_buffer_status =
      ::tflite::task::vision::CreateCppFrameBuffer(frame_buffer);
  if (!cpp_frame_buffer_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_frame_buffer_status.status(), error);
    return nullptr;
  }

  BoundingBoxCpp cc_roi;
  if (roi == nullptr) {
    cc_roi.set_width(frame_buffer->dimension.width);
    cc_roi.set_height(frame_buffer->dimension.height);
  } else {
    cc_roi.set_origin_x(roi->origin_x);
    cc_roi.set_origin_y(roi->origin_y);
    cc_roi.set_width(roi->width);
    cc_roi.set_height(roi->height);
  }

  // fnc_sample(cpp_frame_buffer_status);
  StatusOr<ClassificationResultCpp> cpp_classification_result_status =
      classifier->impl->Classify(*(cpp_frame_buffer_status.value()), cc_roi);

  if (!cpp_classification_result_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_classification_result_status.status(), error);
    return nullptr;
  }

  return GetClassificationResultCStruct(
      cpp_classification_result_status.value());
}

TfLiteClassificationResult* TfLiteImageClassifierClassify(
    const TfLiteImageClassifier* classifier,
    const TfLiteFrameBuffer* frame_buffer, TfLiteSupportError** error) {
  return TfLiteImageClassifierClassifyWithRoi(classifier, frame_buffer, nullptr,
                                              error);
}

void TfLiteImageClassifierDelete(TfLiteImageClassifier* classifier) {
  delete classifier;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
