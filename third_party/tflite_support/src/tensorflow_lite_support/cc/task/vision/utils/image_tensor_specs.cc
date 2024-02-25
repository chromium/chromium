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
#include "tensorflow_lite_support/cc/task/vision/utils/image_tensor_specs.h"

#include "absl/algorithm/container.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/types/optional.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::absl::StatusCode;
using ::tflite::ColorSpaceType_RGB;
using ::tflite::ContentProperties;
using ::tflite::ContentProperties_ImageProperties;
using ::tflite::EnumNameContentProperties;
using ::tflite::ImageProperties;
using ::tflite::TensorMetadata;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::TfLiteEngine;

StatusOr<const TensorMetadata*> GetInputTensorMetadataIfAny(
    const ModelMetadataExtractor& metadata_extractor) {
  if (metadata_extractor.GetModelMetadata() == nullptr ||
      metadata_extractor.GetModelMetadata()->subgraph_metadata() == nullptr) {
    // Some models have no metadata at all (or very partial), so exit early.
    return nullptr;
  } else if (metadata_extractor.GetInputTensorCount() != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Models are assumed to have a single input TensorMetadata.",
        TfLiteSupportStatus::kInvalidNumInputTensorsError);
  }

  const TensorMetadata* metadata = metadata_extractor.GetInputTensorMetadata(0);

  if (metadata == nullptr) {
    // Should never happen.
    return CreateStatusWithPayload(StatusCode::kInternal,
                                   "Input TensorMetadata is null.");
  }

  return metadata;
}

StatusOr<const ImageProperties*> GetImagePropertiesIfAny(
    const TensorMetadata& tensor_metadata) {
  if (tensor_metadata.content() == nullptr ||
      tensor_metadata.content()->content_properties() == nullptr) {
    return nullptr;
  }

  ContentProperties type = tensor_metadata.content()->content_properties_type();

  if (type != ContentProperties_ImageProperties) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat(
            "Expected ImageProperties for tensor ",
            tensor_metadata.name() ? tensor_metadata.name()->str() : "#0",
            ", got ", EnumNameContentProperties(type), "."),
        TfLiteSupportStatus::kMetadataInvalidContentPropertiesError);
  }

  return tensor_metadata.content()->content_properties_as_ImageProperties();
}

StatusOr<absl::optional<NormalizationOptions>> GetNormalizationOptionsIfAny(
    const TensorMetadata& tensor_metadata) {
  TFLITE_ASSIGN_OR_RETURN(
      const tflite::ProcessUnit* normalization_process_unit,
      ModelMetadataExtractor::FindFirstProcessUnit(
          tensor_metadata, tflite::ProcessUnitOptions_NormalizationOptions));
  if (normalization_process_unit == nullptr) {
    return {absl::nullopt};
  }
  const tflite::NormalizationOptions* tf_normalization_options =
      normalization_process_unit->options_as_NormalizationOptions();
  const auto mean_values = tf_normalization_options->mean();
  const auto std_values = tf_normalization_options->std();
  if (mean_values->size() != std_values->size()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("NormalizationOptions: expected mean and std of same "
                     "dimension, got ",
                     mean_values->size(), " and ", std_values->size(), "."),
        TfLiteSupportStatus::kMetadataInvalidProcessUnitsError);
  }
  absl::optional<NormalizationOptions> normalization_options;
  if (mean_values->size() == 1) {
    normalization_options = NormalizationOptions{
        /* mean_values= */ {mean_values->Get(0), mean_values->Get(0),
                            mean_values->Get(0)},
        /* std_values= */
        {std_values->Get(0), std_values->Get(0), std_values->Get(0)},
        /* num_values= */ 1};
  } else if (mean_values->size() == 3) {
    normalization_options = NormalizationOptions{
        /* mean_values= */ {mean_values->Get(0), mean_values->Get(1),
                            mean_values->Get(2)},
        /* std_values= */
        {std_values->Get(0), std_values->Get(1), std_values->Get(2)},
        /* num_values= */ 3};
  } else {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("NormalizationOptions: only 1 or 3 mean and std "
                     "values are supported, got ",
                     mean_values->size(), "."),
        TfLiteSupportStatus::kMetadataInvalidProcessUnitsError);
  }
  return normalization_options;
}

}  // namespace

StatusOr<ImageTensorSpecs> BuildInputImageTensorSpecs(
    const TfLiteEngine::Interpreter& interpreter,
    const tflite::metadata::ModelMetadataExtractor& metadata_extractor) {
  TFLITE_ASSIGN_OR_RETURN(const TensorMetadata* metadata,
                   GetInputTensorMetadataIfAny(metadata_extractor));

  const ImageProperties* props = nullptr;
  absl::optional<NormalizationOptions> normalization_options;
  if (metadata != nullptr) {
    TFLITE_ASSIGN_OR_RETURN(props, GetImagePropertiesIfAny(*metadata));
    TFLITE_ASSIGN_OR_RETURN(normalization_options,
                     GetNormalizationOptionsIfAny(*metadata));
  }

  if (TfLiteEngine::InputCount(&interpreter) != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Models are assumed to have a single input.",
        TfLiteSupportStatus::kInvalidNumInputTensorsError);
  }

  // Input-related specifications.
  const TfLiteTensor* input_tensor = TfLiteEngine::GetInput(&interpreter, 0);
  if (input_tensor->dims->size != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Only 4D tensors in BHWD layout are supported.",
        TfLiteSupportStatus::kInvalidInputTensorDimensionsError);
  }
  static constexpr TfLiteType valid_types[] = {kTfLiteUInt8, kTfLiteFloat32};
  TfLiteType input_type = input_tensor->type;
  if (!absl::c_linear_search(valid_types, input_type)) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat(
            "Type mismatch for input tensor ", input_tensor->name,
            ". Requested one of these types: kTfLiteUint8/kTfLiteFloat32, got ",
            TfLiteTypeGetName(input_type), "."),
        TfLiteSupportStatus::kInvalidInputTensorTypeError);
  }

  // The expected layout is BHWD, i.e. batch x height x width x color
  // See https://www.tensorflow.org/guide/tensors
  const int batch = input_tensor->dims->data[0];
  const int height = input_tensor->dims->data[1];
  const int width = input_tensor->dims->data[2];
  const int depth = input_tensor->dims->data[3];

  if (props != nullptr && props->color_space() != ColorSpaceType_RGB) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Only RGB color space is supported for now.",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (batch != 1 || depth != 3) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("The input tensor should have dimensions 1 x height x "
                     "width x 3. Got ",
                     batch, " x ", height, " x ", width, " x ", depth, "."),
        TfLiteSupportStatus::kInvalidInputTensorDimensionsError);
  }
  int bytes_size = input_tensor->bytes;
  size_t byte_depth =
      input_type == kTfLiteFloat32 ? sizeof(float) : sizeof(uint8);

  // Sanity checks.
  if (input_type == kTfLiteFloat32) {
    if (!normalization_options.has_value()) {
      return CreateStatusWithPayload(
          absl::StatusCode::kNotFound,
          "Input tensor has type kTfLiteFloat32: it requires specifying "
          "NormalizationOptions metadata to preprocess input images.",
          TfLiteSupportStatus::kMetadataMissingNormalizationOptionsError);
    } else if (bytes_size / sizeof(float) %
                   normalization_options.value().num_values !=
               0) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          "The number of elements in the input tensor must be a multiple of "
          "the number of normalization parameters.",
          TfLiteSupportStatus::kInvalidArgumentError);
    }
  }
  if (width <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument, "The input width should be positive.",
        TfLiteSupportStatus::kInvalidInputTensorDimensionsError);
  }
  if (height <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument, "The input height should be positive.",
        TfLiteSupportStatus::kInvalidInputTensorDimensionsError);
  }
  if (bytes_size != height * width * depth * byte_depth) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "The input size in bytes does not correspond to the expected number of "
        "pixels.",
        TfLiteSupportStatus::kInvalidInputTensorSizeError);
  }

  // Note: in the future, additional checks against `props->default_size()`
  // might be added. Also, verify that NormalizationOptions, if any, do specify
  // a single value when color space is grayscale.

  ImageTensorSpecs result;
  result.image_width = width;
  result.image_height = height;
  result.color_space = ColorSpaceType_RGB;
  result.tensor_type = input_type;
  result.normalization_options = normalization_options;

  return result;
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
