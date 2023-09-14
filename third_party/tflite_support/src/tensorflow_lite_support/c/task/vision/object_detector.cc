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

#include "tensorflow_lite_support/c/task/vision/object_detector.h"

#include <memory>

#include "tensorflow_lite_support/c/common_utils.h"
#include "tensorflow_lite_support/c/task/core/utils/base_options_utils.h"
#include "tensorflow_lite_support/c/task/processor/utils/classification_options_utils.h"
#include "tensorflow_lite_support/c/task/vision/utils/frame_buffer_cpp_c_utils.h"
#include "tensorflow_lite_support/cc/task/vision/object_detector.h"
#include "tensorflow_lite_support/cc/task/vision/proto/detections_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/object_detector_options_proto_inc.h"

namespace {
using ::tflite::support::StatusOr;
using DetectionResultCpp = ::tflite::task::vision::DetectionResult;
using DetectionCpp = ::tflite::task::vision::Detection;
using ClassCpp = ::tflite::task::vision::Class;
using BoundingBoxCpp = ::tflite::task::vision::BoundingBox;
using ObjectDetectorCpp = ::tflite::task::vision::ObjectDetector;
using ObjectDetectorOptionsCpp = ::tflite::task::vision::ObjectDetectorOptions;
using FrameBufferCpp = ::tflite::task::vision::FrameBuffer;
using ::tflite::support::TfLiteSupportStatus;

StatusOr<ObjectDetectorOptionsCpp> CreateObjectDetectorCppOptionsFromCOptions(
    const TfLiteObjectDetectorOptions* c_options) {
  if (c_options == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null options."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  ObjectDetectorOptionsCpp cpp_options = {};

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

struct TfLiteObjectDetector {
  std::unique_ptr<ObjectDetectorCpp> impl;
};

TfLiteObjectDetectorOptions TfLiteObjectDetectorOptionsCreate(void) {
  // Use brace-enclosed initializer list will break the Kokoro test.
  TfLiteObjectDetectorOptions options;
  options.classification_options =
      tflite::task::processor::CreateDefaultClassificationOptions();
  options.base_options = tflite::task::core::CreateDefaultBaseOptions();
  return options;
}

TfLiteObjectDetector* TfLiteObjectDetectorFromOptions(
    const TfLiteObjectDetectorOptions* options, TfLiteSupportError** error) {
  StatusOr<ObjectDetectorOptionsCpp> cpp_option_status =
      CreateObjectDetectorCppOptionsFromCOptions(options);

  if (!cpp_option_status.ok()) {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_option_status.status(), error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<ObjectDetectorCpp>> detector_status =
      ObjectDetectorCpp::CreateFromOptions(cpp_option_status.value());

  if (detector_status.ok()) {
    return new TfLiteObjectDetector{.impl = std::move(detector_status.value())};
  } else {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        detector_status.status(), error);
    return nullptr;
  }
}

TfLiteDetectionResult* GetDetectionResultCStruct(
    const DetectionResultCpp& detection_result_cpp) {
  auto c_detections =
      new TfLiteDetection[detection_result_cpp.detections_size()];

  for (int i = 0; i < detection_result_cpp.detections_size(); ++i) {
    const DetectionCpp& detection = detection_result_cpp.detections(i);

    c_detections[i].bounding_box.origin_x = detection.bounding_box().origin_x();
    c_detections[i].bounding_box.origin_y = detection.bounding_box().origin_y();
    c_detections[i].bounding_box.width = detection.bounding_box().width();
    c_detections[i].bounding_box.height = detection.bounding_box().height();

    auto c_categories = new TfLiteCategory[detection.classes_size()];
    c_detections[i].size = detection.classes_size();

    for (int j = 0; j < detection.classes_size(); ++j) {
      const ClassCpp& classification = detection.classes(j);
      c_categories[j].index = classification.index();
      c_categories[j].score = classification.score();

      if (classification.has_class_name())
        c_categories[j].label = strdup(classification.class_name().c_str());
      else
        c_categories[j].label = nullptr;

      if (classification.has_display_name())
        c_categories[j].display_name =
            strdup(classification.display_name().c_str());
      else
        c_categories[j].display_name = nullptr;
    }
    c_detections[i].categories = c_categories;
  }

  auto c_detection_result = new TfLiteDetectionResult;
  c_detection_result->detections = c_detections;
  c_detection_result->size = detection_result_cpp.detections_size();

  return c_detection_result;
}

TfLiteDetectionResult* TfLiteObjectDetectorDetect(
    const TfLiteObjectDetector* detector, const TfLiteFrameBuffer* frame_buffer,
    TfLiteSupportError** error) {
  if (detector == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null object detector.", error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<FrameBufferCpp>> cpp_frame_buffer_status =
      ::tflite::task::vision::CreateCppFrameBuffer(frame_buffer);
  if (!cpp_frame_buffer_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_frame_buffer_status.status(), error);
    return nullptr;
  }

  StatusOr<DetectionResultCpp> cpp_detection_result_status =
      detector->impl->Detect(*(cpp_frame_buffer_status.value()));
  if (!cpp_detection_result_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_detection_result_status.status(), error);
    return nullptr;
  }

  return GetDetectionResultCStruct(cpp_detection_result_status.value());
}

void TfLiteObjectDetectorDelete(TfLiteObjectDetector* detector) {
  delete detector;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
