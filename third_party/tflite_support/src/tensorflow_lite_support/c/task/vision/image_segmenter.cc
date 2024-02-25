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

#include "tensorflow_lite_support/c/task/vision/image_segmenter.h"

#include <memory>

#include "tensorflow_lite_support/c/common_utils.h"
#include "tensorflow_lite_support/c/task/core/utils/base_options_utils.h"
#include "tensorflow_lite_support/c/task/vision/utils/frame_buffer_cpp_c_utils.h"
#include "tensorflow_lite_support/cc/task/vision/image_segmenter.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_segmenter_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/segmentations_proto_inc.h"

namespace {
using ::tflite::support::StatusOr;
using SegmentationResultCpp = ::tflite::task::vision::SegmentationResult;
using SegmentationCpp = ::tflite::task::vision::Segmentation;
using ColoredLabelCpp = ::tflite::task::vision::Segmentation_ColoredLabel;
using ImageSegmenterCpp = ::tflite::task::vision::ImageSegmenter;
using ImageSegmenterOptionsCpp = ::tflite::task::vision::ImageSegmenterOptions;
using FrameBufferCpp = ::tflite::task::vision::FrameBuffer;
using OutputTypeCpp = ::tflite::task::vision::ImageSegmenterOptions_OutputType;
using ::tflite::support::TfLiteSupportStatus;

StatusOr<ImageSegmenterOptionsCpp> CreateImageSegmenterCppOptionsFromCOptions(
    const TfLiteImageSegmenterOptions* c_options) {
  if (c_options == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Expected non null options."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  ImageSegmenterOptionsCpp cpp_options = {};

  // More file sources can be added in else ifs
  if (c_options->base_options.model_file.file_path)
    cpp_options.mutable_base_options()->mutable_model_file()->set_file_name(
        c_options->base_options.model_file.file_path);

  // c_options->base_options.compute_settings.num_threads is expected to be
  // set to value > 0 or -1. Otherwise invoking
  // ImageSegmenterCpp::CreateFromOptions() results in a not ok status.
  cpp_options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(
          c_options->base_options.compute_settings.cpu_settings.num_threads);

  cpp_options.set_output_type((OutputTypeCpp)c_options->output_type);

  // Check needed since setting a nullptr for this field results in a segfault
  // on invocation of ImageSegmenterCpp::CreateFromOptions().
  if (c_options->display_names_locale) {
    cpp_options.set_display_names_locale(c_options->display_names_locale);
  }

  return cpp_options;
}
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct TfLiteImageSegmenter {
  std::unique_ptr<ImageSegmenterCpp> impl;
};

TfLiteImageSegmenterOptions TfLiteImageSegmenterOptionsCreate(void) {
  // Use brace-enclosed initializer list will break the Kokoro test.
  TfLiteImageSegmenterOptions options;
  options.base_options = tflite::task::core::CreateDefaultBaseOptions();
  options.display_names_locale = nullptr;
  options.output_type = kCategoryMask;

  return options;
}

TfLiteImageSegmenter* TfLiteImageSegmenterFromOptions(
    const TfLiteImageSegmenterOptions* options, TfLiteSupportError** error) {
  StatusOr<ImageSegmenterOptionsCpp> cpp_option_status =
      CreateImageSegmenterCppOptionsFromCOptions(options);

  if (!cpp_option_status.ok()) {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_option_status.status(), error);
    return nullptr;
  }
  StatusOr<std::unique_ptr<ImageSegmenterCpp>> segmenter_status =
      ImageSegmenterCpp::CreateFromOptions(cpp_option_status.value());

  if (segmenter_status.ok()) {
    return new TfLiteImageSegmenter{.impl =
                                        std::move(segmenter_status.value())};
  } else {
    ::tflite::support::CreateTfLiteSupportErrorWithStatus(
        segmenter_status.status(), error);
    return nullptr;
  }
}

TfLiteSegmentationResult* GetSegmentationResultCStruct(
    const SegmentationResultCpp& segmentation_result_cpp) {
  auto c_segmentations =
      new TfLiteSegmentation[segmentation_result_cpp.segmentation_size()]();

  for (int i = 0; i < segmentation_result_cpp.segmentation_size(); ++i) {
    const SegmentationCpp& segmentation =
        segmentation_result_cpp.segmentation(i);

    c_segmentations[i].width = (int)segmentation.width();
    c_segmentations[i].height = (int)segmentation.height();
    c_segmentations[i].colored_labels_size = segmentation.colored_labels_size();

    auto c_colored_labels =
        new TfLiteColoredLabel[segmentation.colored_labels_size()]();

    if (segmentation.has_category_mask()) {
      c_segmentations[i].category_mask =
          new uint8_t[segmentation.width() * segmentation.height()];
      std::memcpy(
          c_segmentations[i].category_mask,
          reinterpret_cast<const uint8_t*>(segmentation.category_mask().data()),
          segmentation.width() * segmentation.height() * sizeof(uint8_t));

    } else if (segmentation.has_confidence_masks()) {
      c_segmentations[i].confidence_masks =
          new float*[segmentation.colored_labels_size()];
      for (int k = 0; k < segmentation.colored_labels_size(); ++k) {
        c_segmentations[i].confidence_masks[k] =
            new float[segmentation.width() * segmentation.height()];

        std::memcpy(
            c_segmentations[i].confidence_masks[k],
            reinterpret_cast<const float*>(segmentation.confidence_masks()
                                               .confidence_mask(k)
                                               .value()
                                               .data()),
            segmentation.width() * segmentation.height() * sizeof(float));
      }
    }

    for (int j = 0; j < segmentation.colored_labels_size(); ++j) {
      const ColoredLabelCpp& colored_label = segmentation.colored_labels(j);
      c_colored_labels[j].r = colored_label.r();
      c_colored_labels[j].g = colored_label.g();
      c_colored_labels[j].b = colored_label.b();

      if (colored_label.has_class_name()) {
        c_colored_labels[j].label = strdup(colored_label.class_name().c_str());
      }

      if (colored_label.has_display_name()) {
        c_colored_labels[j].display_name =
            strdup(colored_label.display_name().c_str());
      }
    }
    c_segmentations[i].colored_labels = c_colored_labels;
  }

  auto c_segmentation_result = new TfLiteSegmentationResult;
  c_segmentation_result->segmentations = c_segmentations;
  c_segmentation_result->size = segmentation_result_cpp.segmentation_size();

  return c_segmentation_result;
}

TfLiteSegmentationResult* TfLiteImageSegmenterSegment(
    const TfLiteImageSegmenter* segmenter,
    const TfLiteFrameBuffer* frame_buffer, TfLiteSupportError** error) {
  if (segmenter == nullptr) {
    tflite::support::CreateTfLiteSupportError(
        kInvalidArgumentError, "Expected non null image segmenter.", error);
    return nullptr;
  }

  StatusOr<std::unique_ptr<FrameBufferCpp>> cpp_frame_buffer_status =
      ::tflite::task::vision::CreateCppFrameBuffer(frame_buffer);
  if (!cpp_frame_buffer_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_frame_buffer_status.status(), error);
    return nullptr;
  }

  StatusOr<SegmentationResultCpp> cpp_segmentation_result_status =
      segmenter->impl->Segment(*(cpp_frame_buffer_status.value()));

  if (!cpp_segmentation_result_status.ok()) {
    tflite::support::CreateTfLiteSupportErrorWithStatus(
        cpp_segmentation_result_status.status(), error);
    return nullptr;
  }

  return GetSegmentationResultCStruct(cpp_segmentation_result_status.value());
}

void TfLiteImageSegmenterDelete(TfLiteImageSegmenter* segmenter) {
  delete segmenter;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
