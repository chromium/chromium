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
#include "tensorflow_lite_support/cc/task/core/classification_head.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace core {

using ::absl::StatusCode;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

StatusOr<ClassificationHead> BuildClassificationHead(
    const tflite::metadata::ModelMetadataExtractor& metadata_extractor,
    const tflite::TensorMetadata& output_tensor_metadata,
    absl::string_view display_names_locale) {
  ClassificationHead head;
  if (output_tensor_metadata.name() != nullptr) {
    head.name = output_tensor_metadata.name()->str();
  }

  // Build label map, if present.
  const std::string labels_filename =
      ModelMetadataExtractor::FindFirstAssociatedFileName(
          output_tensor_metadata,
          tflite::AssociatedFileType_TENSOR_AXIS_LABELS);
  if (!labels_filename.empty()) {
    TFLITE_ASSIGN_OR_RETURN(absl::string_view labels_file,
                     metadata_extractor.GetAssociatedFile(labels_filename));
    const std::string display_names_filename =
        ModelMetadataExtractor::FindFirstAssociatedFileName(
            output_tensor_metadata,
            tflite::AssociatedFileType_TENSOR_AXIS_LABELS,
            display_names_locale);
    absl::string_view display_names_file;
    if (!display_names_filename.empty()) {
      TFLITE_ASSIGN_OR_RETURN(display_names_file, metadata_extractor.GetAssociatedFile(
                                               display_names_filename));
    }
    TFLITE_ASSIGN_OR_RETURN(head.label_map_items,
                     BuildLabelMapFromFiles(labels_file, display_names_file));
  }

  // Set score threshold, if present.
  TFLITE_ASSIGN_OR_RETURN(const tflite::ProcessUnit* score_thresholding_process_unit,
                   ModelMetadataExtractor::FindFirstProcessUnit(
                       output_tensor_metadata,
                       tflite::ProcessUnitOptions_ScoreThresholdingOptions));
  if (score_thresholding_process_unit != nullptr) {
    head.score_threshold =
        score_thresholding_process_unit->options_as_ScoreThresholdingOptions()
            ->global_score_threshold();
  }

  // Build score calibration parameters, if present.
  TFLITE_ASSIGN_OR_RETURN(const tflite::ProcessUnit* score_calibration_process_unit,
                   ModelMetadataExtractor::FindFirstProcessUnit(
                       output_tensor_metadata,
                       tflite::ProcessUnitOptions_ScoreCalibrationOptions));
  if (score_calibration_process_unit != nullptr) {
    if (labels_filename.empty()) {
      return CreateStatusWithPayload(
          StatusCode::kNotFound,
          "Using ScoreCalibrationOptions requires a label map to be provided "
          "as TENSOR_AXIS_LABELS associated file.",
          TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError);
    }
    const std::string score_calibration_filename =
        ModelMetadataExtractor::FindFirstAssociatedFileName(
            output_tensor_metadata,
            tflite::AssociatedFileType_TENSOR_AXIS_SCORE_CALIBRATION);
    if (score_calibration_filename.empty()) {
      return CreateStatusWithPayload(
          StatusCode::kNotFound,
          "Found ScoreCalibrationOptions but missing required associated "
          "parameters file with type TENSOR_AXIS_SCORE_CALIBRATION.",
          TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError);
    }
    TFLITE_ASSIGN_OR_RETURN(
        absl::string_view score_calibration_file,
        metadata_extractor.GetAssociatedFile(score_calibration_filename));
    TFLITE_ASSIGN_OR_RETURN(SigmoidCalibrationParameters sigmoid_params,
                     BuildSigmoidCalibrationParams(
                         *score_calibration_process_unit
                              ->options_as_ScoreCalibrationOptions(),
                         score_calibration_file, head.label_map_items));
    head.calibration_params = sigmoid_params;
  }

  return head;
}

}  // namespace core
}  // namespace task
}  // namespace tflite
