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

#include "tensorflow_lite_support/cc/task/vision/object_detector.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <glog/logging.h>
#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/core/label_map_item.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/score_calibration.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace vision {

namespace {

using ::absl::StatusCode;
using ::tflite::BoundingBoxProperties;
using ::tflite::ContentProperties;
using ::tflite::ContentProperties_BoundingBoxProperties;
using ::tflite::EnumNameContentProperties;
using ::tflite::ProcessUnit;
using ::tflite::ProcessUnitOptions_ScoreThresholdingOptions;
using ::tflite::TensorMetadata;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::AssertAndReturnTypedTensor;
using ::tflite::task::core::FindTensorIndexByMetadataName;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

// The expected number of dimensions of the 4 output tensors, representing in
// that order: locations, categories, scores, num_results. The order is
// coming from the TFLite custom NMS op for object detection post-processing
// shown in
// https://github.com/tensorflow/tensorflow/blob/1c419b231b622bd9e9685682545e9064f0fbb42a/tensorflow/lite/kernels/detection_postprocess.cc#L47.
static constexpr int kOutputTensorsExpectedDims[4] = {3, 2, 2, 1};
constexpr int kDefaultLocationsIndex = 0;
constexpr int kDefaultClassesIndex = 1;
constexpr int kDefaultScoresIndex = 2;
constexpr int kDefaultNumResultsIndex = 3;

constexpr float kDefaultScoreThreshold = std::numeric_limits<float>::lowest();

constexpr char kLocationTensorName[] = "location";
constexpr char kCategoryTensorName[] = "category";
constexpr char kScoreTensorName[] = "score";
constexpr char kNumberOfDetectionsTensorName[] = "number of detections";

StatusOr<const BoundingBoxProperties*> GetBoundingBoxProperties(
    const TensorMetadata& tensor_metadata) {
  if (tensor_metadata.content() == nullptr ||
      tensor_metadata.content()->content_properties() == nullptr) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Expected BoundingBoxProperties for tensor %s, found none.",
            tensor_metadata.name() ? tensor_metadata.name()->str() : "#0"),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  ContentProperties type = tensor_metadata.content()->content_properties_type();
  if (type != ContentProperties_BoundingBoxProperties) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Expected BoundingBoxProperties for tensor %s, found %s.",
            tensor_metadata.name() ? tensor_metadata.name()->str() : "#0",
            EnumNameContentProperties(type)),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  const BoundingBoxProperties* properties =
      tensor_metadata.content()->content_properties_as_BoundingBoxProperties();

  // Mobile SSD only supports "BOUNDARIES" bounding box type.
  if (properties->type() != tflite::BoundingBoxType_BOUNDARIES) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Mobile SSD only supports BoundingBoxType BOUNDARIES, found %s",
            tflite::EnumNameBoundingBoxType(properties->type())),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  // Mobile SSD only supports "RATIO" coordinates type.
  if (properties->coordinate_type() != tflite::CoordinateType_RATIO) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Mobile SSD only supports CoordinateType RATIO, found %s",
            tflite::EnumNameCoordinateType(properties->coordinate_type())),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  // Index is optional, but must contain 4 values if present.
  if (properties->index() != nullptr && properties->index()->size() != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Expected BoundingBoxProperties index to contain 4 values, found "
            "%d",
            properties->index()->size()),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  return properties;
}

StatusOr<std::vector<LabelMapItem>> GetLabelMapIfAny(
    const ModelMetadataExtractor& metadata_extractor,
    const TensorMetadata& tensor_metadata, absl::string_view locale) {
  const std::string labels_filename =
      ModelMetadataExtractor::FindFirstAssociatedFileName(
          tensor_metadata, tflite::AssociatedFileType_TENSOR_VALUE_LABELS);
  if (labels_filename.empty()) {
    return std::vector<LabelMapItem>();
  }
  TFLITE_ASSIGN_OR_RETURN(absl::string_view labels_file,
                   metadata_extractor.GetAssociatedFile(labels_filename));
  const std::string display_names_filename =
      ModelMetadataExtractor::FindFirstAssociatedFileName(
          tensor_metadata, tflite::AssociatedFileType_TENSOR_VALUE_LABELS,
          locale);
  absl::string_view display_names_file;
  if (!display_names_filename.empty()) {
    TFLITE_ASSIGN_OR_RETURN(display_names_file, metadata_extractor.GetAssociatedFile(
                                             display_names_filename));
  }
  return BuildLabelMapFromFiles(labels_file, display_names_file);
}

StatusOr<float> GetScoreThreshold(
    const ModelMetadataExtractor& metadata_extractor,
    const TensorMetadata& tensor_metadata) {
  TFLITE_ASSIGN_OR_RETURN(
      const ProcessUnit* score_thresholding_process_unit,
      metadata_extractor.FindFirstProcessUnit(
          tensor_metadata, ProcessUnitOptions_ScoreThresholdingOptions));
  if (score_thresholding_process_unit == nullptr) {
    return kDefaultScoreThreshold;
  }
  return score_thresholding_process_unit->options_as_ScoreThresholdingOptions()
      ->global_score_threshold();
}

// Use tensor names in metadata to get the output order.
std::vector<int> GetOutputIndices(
    const flatbuffers::Vector<flatbuffers::Offset<TensorMetadata>>*
        tensor_metadatas) {
  std::vector<int> output_indices = {
      FindTensorIndexByMetadataName(tensor_metadatas, kLocationTensorName),
      FindTensorIndexByMetadataName(tensor_metadatas, kCategoryTensorName),
      FindTensorIndexByMetadataName(tensor_metadatas, kScoreTensorName),
      FindTensorIndexByMetadataName(tensor_metadatas,
                                    kNumberOfDetectionsTensorName)};

  for (int i = 0; i < 4; i++) {
    int output_index = output_indices[i];
    // If tensor name is not found, set the default output indices.
    if (output_index == -1) {
      LOG(WARNING) << absl::StrFormat(
          "You don't seem to be matching tensor names in metadata list. The "
          "tensor name \"%s\" at index %d in the model metadata doesn't match "
          "the available output names: [\"%s\", \"%s\", \"%s\", \"%s\"].",
          tensor_metadatas->Get(i)->name()->c_str(), i, kLocationTensorName,
          kCategoryTensorName, kScoreTensorName, kNumberOfDetectionsTensorName);
      output_indices = {kDefaultLocationsIndex, kDefaultClassesIndex,
                        kDefaultScoresIndex, kDefaultNumResultsIndex};
      return output_indices;
    }
  }
  return output_indices;
}

absl::Status SanityCheckOutputTensors(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::vector<int>& output_indices) {
  if (output_tensors.size() != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Expected 4 output tensors, found %d",
                        output_tensors.size()));
  }

  // Get number of results.
  const TfLiteTensor* num_results_tensor = output_tensors[output_indices[3]];
  if (num_results_tensor->dims->data[0] != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat(
            "Expected tensor with dimensions [1] at index 3, found [%d]",
            num_results_tensor->dims->data[0]));
  }

  TFLITE_ASSIGN_OR_RETURN(float* num_results_data,
                   AssertAndReturnTypedTensor<float>(num_results_tensor));
  int num_results = static_cast<int>(num_results_data[0]);

  const TfLiteTensor* location_tensor = output_tensors[output_indices[0]];
  // Check dimensions for the other tensors are correct.
  if (location_tensor->dims->data[0] != 1 ||
      location_tensor->dims->data[1] < num_results ||
      location_tensor->dims->data[2] != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat(
            "Expected locations tensor with dimensions [1, num_detected_boxes, "
            "4] at index 0, num_detected_boxes >= %d, found [%d,%d,%d].",
            num_results, location_tensor->dims->data[0],
            location_tensor->dims->data[1], location_tensor->dims->data[2]));
  }

  const TfLiteTensor* class_tensor = output_tensors[output_indices[1]];
  if (class_tensor->dims->data[0] != 1 ||
      class_tensor->dims->data[1] < num_results) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat(
            "Expected classes tensor with dimensions [1, num_detected_boxes] "
            "at index 1, num_detected_boxes >= %d, found [%d,%d].",
            num_results, class_tensor->dims->data[0],
            class_tensor->dims->data[1]));
  }

  const TfLiteTensor* scores_tensor = output_tensors[output_indices[2]];
  if (scores_tensor->dims->data[0] != 1 ||
      scores_tensor->dims->data[1] < num_results) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat(
            "Expected scores tensor with dimensions [1, num_detected_boxes] "
            "at index 2, num_detected_boxes >= %d, found [%d,%d].",
            num_results, scores_tensor->dims->data[0],
            scores_tensor->dims->data[1]));
  }

  return absl::OkStatus();
}

}  // namespace

/* static */
absl::Status ObjectDetector::SanityCheckOptions(
    const ObjectDetectorOptions& options) {
  int num_input_models = (options.base_options().has_model_file() ? 1 : 0) +
                         (options.has_model_file_with_metadata() ? 1 : 0);
  if (num_input_models != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found %d.",
                        num_input_models),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.max_results() == 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Invalid `max_results` option: value must be != 0",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.class_name_whitelist_size() > 0 &&
      options.class_name_blacklist_size() > 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "`class_name_whitelist` and `class_name_blacklist` are mutually "
        "exclusive options.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.num_threads() == 0 || options.num_threads() < -1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "`num_threads` must be greater than 0 or equal to -1.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

/* static */
StatusOr<std::unique_ptr<ObjectDetector>> ObjectDetector::CreateFromOptions(
    const ObjectDetectorOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the constructed object.
  auto options_copy = absl::make_unique<ObjectDetectorOptions>(options);

  std::unique_ptr<ObjectDetector> object_detector;
  if (options_copy->has_model_file_with_metadata()) {
    TFLITE_ASSIGN_OR_RETURN(
        object_detector,
        TaskAPIFactory::CreateFromExternalFileProto<ObjectDetector>(
            &options_copy->model_file_with_metadata(), std::move(resolver),
            options_copy->num_threads(), options_copy->compute_settings()));
  } else if (options_copy->base_options().has_model_file()) {
    TFLITE_ASSIGN_OR_RETURN(object_detector,
                     TaskAPIFactory::CreateFromBaseOptions<ObjectDetector>(
                         &options_copy->base_options(), std::move(resolver)));
  } else {
    // Should never happen because of SanityCheckOptions.
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  TFLITE_RETURN_IF_ERROR(object_detector->Init(std::move(options_copy)));

  return object_detector;
}

absl::Status ObjectDetector::Init(
    std::unique_ptr<ObjectDetectorOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Perform pre-initialization actions (by default, sets the process engine for
  // image pre-processing to kLibyuv as a sane default).
  TFLITE_RETURN_IF_ERROR(PreInit());

  // Sanity check and set inputs and outputs.
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());
  TFLITE_RETURN_IF_ERROR(CheckAndSetOutputs());

  // Initialize class whitelisting/blacklisting, if any.
  TFLITE_RETURN_IF_ERROR(CheckAndSetClassIndexSet());

  // Perform final initialization (by default, initialize score calibration
  // parameters, if any).
  TFLITE_RETURN_IF_ERROR(PostInit());

  return absl::OkStatus();
}

absl::Status ObjectDetector::PreInit() {
  SetProcessEngine(FrameBufferUtils::ProcessEngine::kLibyuv);
  return absl::OkStatus();
}

absl::Status ObjectDetector::PostInit() { return InitScoreCalibrations(); }

StatusOr<SigmoidCalibrationParameters> BuildCalibrationParametersIfAny(
    const tflite::metadata::ModelMetadataExtractor& metadata_extractor,
    const tflite::TensorMetadata& output_tensor_metadata,
    const std::vector<LabelMapItem>& label_map_items,
    bool* has_score_calibration) {
  SigmoidCalibrationParameters sigmoid_params;
  *has_score_calibration = false;

  // Build score calibration parameters, if present.
  //
  // TODO(tianlin): This code is similar to the block of classification_head.cc
  // that does sanity checks and builds sigmoid calibration params in:
  //   https://github.com/tensorflow/tflite-support/blob/64e044408f3d3654de7fc10bca401ed900649ca3/tensorflow_lite_support/cc/task/vision/core/classification_head.cc#L75-L107
  // Consider to refactor it and reuse the same function.
  TFLITE_ASSIGN_OR_RETURN(const tflite::ProcessUnit* score_calibration_process_unit,
                   ModelMetadataExtractor::FindFirstProcessUnit(
                       output_tensor_metadata,
                       tflite::ProcessUnitOptions_ScoreCalibrationOptions));
  if (score_calibration_process_unit != nullptr) {
    const std::string score_calibration_filename =
        ModelMetadataExtractor::FindFirstAssociatedFileName(
            output_tensor_metadata,
            tflite::AssociatedFileType_TENSOR_AXIS_SCORE_CALIBRATION);
    TFLITE_ASSIGN_OR_RETURN(
        absl::string_view score_calibration_file,
        metadata_extractor.GetAssociatedFile(score_calibration_filename));

    // Set has_score_calibration to true, only if sigmoid_params is built.
    TFLITE_ASSIGN_OR_RETURN(sigmoid_params,
                     BuildSigmoidCalibrationParams(
                         *score_calibration_process_unit
                              ->options_as_ScoreCalibrationOptions(),
                         score_calibration_file, label_map_items));
    *has_score_calibration = true;
  }
  return sigmoid_params;
}

absl::Status ObjectDetector::InitScoreCalibrations() {
  StatusOr<SigmoidCalibrationParameters> calibration_params_status;
  bool has_score_calibration = false;

  // Search the output tensor metadata, can try to get calibration_params.
  const ModelMetadataExtractor* metadata_extractor =
      GetTfLiteEngine()->metadata_extractor();
  const flatbuffers::Vector<flatbuffers::Offset<tflite::TensorMetadata>>*
      output_tensor_metadata = metadata_extractor->GetOutputTensorMetadata();
  const tflite::TensorMetadata* output_tensor =
      output_tensor_metadata->Get(kDefaultScoresIndex);
  TFLITE_ASSIGN_OR_RETURN(
      auto calibration_params,
      BuildCalibrationParametersIfAny(*metadata_extractor, *output_tensor,
                                      label_map_, &has_score_calibration));

  // If no calibration_params is found, just skip score calibration.
  if (!has_score_calibration) {
    return absl::OkStatus();
  }

  score_calibration_ = absl::make_unique<ScoreCalibration>();
  if (score_calibration_ == nullptr) {
    return CreateStatusWithPayload(
        StatusCode::kInternal, "Could not create score calibration object.");
  }
  TFLITE_RETURN_IF_ERROR(
      score_calibration_->InitializeFromParameters(calibration_params));
  return absl::OkStatus();
}

absl::Status ObjectDetector::CheckAndSetOutputs() {
  // First, sanity checks on the model itself.
  const TfLiteEngine::Interpreter* interpreter =
      GetTfLiteEngine()->interpreter();
  // Check the number of output tensors.
  if (TfLiteEngine::OutputCount(interpreter) != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Mobile SSD models are expected to have exactly 4 "
                        "outputs, found %d",
                        TfLiteEngine::OutputCount(interpreter)),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }

  // Now, perform sanity checks and extract metadata.
  const ModelMetadataExtractor* metadata_extractor =
      GetTfLiteEngine()->metadata_extractor();
  // Check that metadata is available.
  if (metadata_extractor->GetModelMetadata() == nullptr ||
      metadata_extractor->GetModelMetadata()->subgraph_metadata() == nullptr) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Object detection models require TFLite "
                                   "Model Metadata but none was found",
                                   TfLiteSupportStatus::kMetadataNotFoundError);
  }
  // Check output tensor metadata is present and consistent with model.
  auto output_tensors_metadata = metadata_extractor->GetOutputTensorMetadata();
  if (output_tensors_metadata == nullptr ||
      output_tensors_metadata->size() != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Mismatch between number of output tensors (4) and output tensors "
            "metadata (%d).",
            output_tensors_metadata == nullptr
                ? 0
                : output_tensors_metadata->size()),
        TfLiteSupportStatus::kMetadataInconsistencyError);
  }

  output_indices_ = GetOutputIndices(output_tensors_metadata);

  // Extract mandatory BoundingBoxProperties for easier access at
  // post-processing time, performing sanity checks on the fly.
  TFLITE_ASSIGN_OR_RETURN(const BoundingBoxProperties* bounding_box_properties,
                   GetBoundingBoxProperties(
                       *output_tensors_metadata->Get(output_indices_[0])));
  if (bounding_box_properties->index() == nullptr) {
    bounding_box_corners_order_ = {0, 1, 2, 3};
  } else {
    auto bounding_box_index = bounding_box_properties->index();
    bounding_box_corners_order_ = {
        bounding_box_index->Get(0),
        bounding_box_index->Get(1),
        bounding_box_index->Get(2),
        bounding_box_index->Get(3),
    };
  }

  // Build label map (if available) from metadata.
  TFLITE_ASSIGN_OR_RETURN(
      label_map_,
      GetLabelMapIfAny(*metadata_extractor,
                       *output_tensors_metadata->Get(output_indices_[1]),
                       options_->display_names_locale()));

  // Set score threshold.
  if (options_->has_score_threshold()) {
    score_threshold_ = options_->score_threshold();
  } else {
    TFLITE_ASSIGN_OR_RETURN(
        score_threshold_,
        GetScoreThreshold(*metadata_extractor,
                          *output_tensors_metadata->Get(output_indices_[2])));
  }

  // Check tensor dimensions and batch size.
  for (int i = 0; i < 4; ++i) {
    std::size_t j = output_indices_[i];
    const TfLiteTensor* tensor = TfLiteEngine::GetOutput(interpreter, j);
    if (tensor->dims->size != kOutputTensorsExpectedDims[i]) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Output tensor at index %d is expected to "
                          "have %d dimensions, found %d.",
                          j, kOutputTensorsExpectedDims[i], tensor->dims->size),
          TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
    }
    if (tensor->dims->data[0] != 1) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Expected batch size of 1, found %d.",
                          tensor->dims->data[0]),
          TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectDetector::CheckAndSetClassIndexSet() {
  // Exit early if no blacklist/whitelist.
  if (options_->class_name_blacklist_size() == 0 &&
      options_->class_name_whitelist_size() == 0) {
    return absl::OkStatus();
  }
  // Label map is mandatory.
  if (label_map_.empty()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Using `class_name_whitelist` or `class_name_blacklist` requires "
        "labels to be present in the TFLite Model Metadata but none was found.",
        TfLiteSupportStatus::kMetadataMissingLabelsError);
  }

  class_index_set_.is_whitelist = options_->class_name_whitelist_size() > 0;
  const auto& class_names = class_index_set_.is_whitelist
                                ? options_->class_name_whitelist()
                                : options_->class_name_blacklist();
  class_index_set_.values.clear();
  for (const auto& class_name : class_names) {
    int index = -1;
    for (int i = 0; i < label_map_.size(); ++i) {
      if (label_map_[i].name == class_name) {
        index = i;
        break;
      }
    }
    // Ignore duplicate or unknown classes.
    if (index < 0 || class_index_set_.values.contains(index)) {
      continue;
    }
    class_index_set_.values.insert(index);
  }

  if (class_index_set_.values.empty()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Invalid class names specified via `class_name_%s`: none match "
            "with model labels.",
            class_index_set_.is_whitelist ? "whitelist" : "blacklist"),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  return absl::OkStatus();
}

StatusOr<DetectionResult> ObjectDetector::Detect(
    const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return InferWithFallback(frame_buffer, roi);
}

StatusOr<DetectionResult> ObjectDetector::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const FrameBuffer& frame_buffer, const BoundingBox& /*roi*/) {
  // Most of the checks here should never happen, as outputs have been validated
  // at construction time. Checking nonetheless and returning internal errors if
  // something bad happens.
  TFLITE_RETURN_IF_ERROR(SanityCheckOutputTensors(output_tensors, output_indices_));

  // Get number of available results.
  TFLITE_ASSIGN_OR_RETURN(
      float* num_results_data,
      AssertAndReturnTypedTensor<float>(output_tensors[output_indices_[3]]));
  const int num_results = static_cast<int>(num_results_data[0]);
  // Compute number of max results to return.
  const int max_results = options_->max_results() > 0
                              ? std::min(options_->max_results(), num_results)
                              : num_results;
  // The dimensions of the upright (i.e. rotated according to its orientation)
  // input frame.
  FrameBuffer::Dimension upright_input_frame_dimensions =
      frame_buffer.dimension();
  if (RequireDimensionSwap(frame_buffer.orientation(),
                           FrameBuffer::Orientation::kTopLeft)) {
    upright_input_frame_dimensions.Swap();
  }

  TFLITE_ASSIGN_OR_RETURN(
      const float* locations,
      AssertAndReturnTypedTensor<float>(output_tensors[output_indices_[0]]));
  TFLITE_ASSIGN_OR_RETURN(
      const float* classes,
      AssertAndReturnTypedTensor<float>(output_tensors[output_indices_[1]]));
  TFLITE_ASSIGN_OR_RETURN(
      const float* scores,
      AssertAndReturnTypedTensor<float>(output_tensors[output_indices_[2]]));
  DetectionResult results;
  for (int i = 0; i < num_results; ++i) {
    const int class_index = static_cast<int>(classes[i]);
    if (!IsClassIndexAllowed(class_index)) {
      continue;
    }

    float score = scores[i];
    // Calibrate score only if score_calibration_ is presented.
    if (score_calibration_ != nullptr) {
      const std::string& class_name = label_map_[class_index].name;
      score = score_calibration_->ComputeCalibratedScore(class_name, score);
    }
    if (score <= score_threshold_) {
      continue;
    }

    Detection* detection = results.add_detections();
    // Denormalize the bounding box cooordinates in the upright frame
    // coordinates system, then rotate back from frame_buffer.orientation() to
    // the unrotated frame of reference coordinates system (i.e. with
    // orientation = kTopLeft).
    *detection->mutable_bounding_box() = OrientAndDenormalizeBoundingBox(
        /*from_left=*/locations[4 * i + bounding_box_corners_order_[0]],
        /*from_top=*/locations[4 * i + bounding_box_corners_order_[1]],
        /*from_right=*/locations[4 * i + bounding_box_corners_order_[2]],
        /*from_bottom=*/locations[4 * i + bounding_box_corners_order_[3]],
        /*from_orientation=*/frame_buffer.orientation(),
        /*to_orientation=*/FrameBuffer::Orientation::kTopLeft,
        /*from_dimension=*/upright_input_frame_dimensions);
    Class* detection_class = detection->add_classes();
    detection_class->set_index(class_index);
    detection_class->set_score(score);
    if (results.detections_size() == max_results) {
      break;
    }
  }

  if (!label_map_.empty()) {
    TFLITE_RETURN_IF_ERROR(FillResultsFromLabelMap(&results));
  }

  return results;
}

bool ObjectDetector::IsClassIndexAllowed(int class_index) {
  if (class_index_set_.values.empty()) {
    return true;
  }
  if (class_index_set_.is_whitelist) {
    return class_index_set_.values.contains(class_index);
  } else {
    return !class_index_set_.values.contains(class_index);
  }
}

absl::Status ObjectDetector::FillResultsFromLabelMap(DetectionResult* result) {
  for (int i = 0; i < result->detections_size(); ++i) {
    Detection* detection = result->mutable_detections(i);
    for (int j = 0; j < detection->classes_size(); ++j) {
      Class* detection_class = detection->mutable_classes(j);
      const int index = detection_class->index();
      if (index >= label_map_.size()) {
        return CreateStatusWithPayload(
            StatusCode::kInvalidArgument,
            absl::StrFormat(
                "Label map does not contain enough elements: model returned "
                "class index %d but label map only contains %d elements.",
                index, label_map_.size()),
            TfLiteSupportStatus::kMetadataInconsistencyError);
      }
      std::string name = label_map_[index].name;
      if (!name.empty()) {
        detection_class->set_class_name(name);
      }
      std::string display_name = label_map_[index].display_name;
      if (!display_name.empty()) {
        detection_class->set_display_name(display_name);
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
