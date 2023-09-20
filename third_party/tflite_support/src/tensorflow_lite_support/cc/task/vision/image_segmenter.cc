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

#include "tensorflow_lite_support/cc/task/vision/image_segmenter.h"

#include <algorithm>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/c/common.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace vision {

namespace {

using ::absl::StatusCode;
using ::tflite::TensorMetadata;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::AssertAndReturnTypedTensor;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

// The maximum number of labels allowed in the labelmap. This is because so far
// segmentation masks are stored with 8 bit per pixel (flattened byte array).
constexpr uint32 kMaxNumClasses = 256;

// The colormap used to fill `ColoredLabel`-s, as a flattened array of 256 {R,
// G, B} components.
constexpr uint8 kColorMap[768] = {
    0,   0,   0,   128, 0,   0,   0,   128, 0,   128, 128, 0,   0,   0,   128,
    128, 0,   128, 0,   128, 128, 128, 128, 128, 64,  0,   0,   192, 0,   0,
    64,  128, 0,   192, 128, 0,   64,  0,   128, 192, 0,   128, 64,  128, 128,
    192, 128, 128, 0,   64,  0,   128, 64,  0,   0,   192, 0,   128, 192, 0,
    0,   64,  128, 128, 64,  128, 0,   192, 128, 128, 192, 128, 64,  64,  0,
    192, 64,  0,   64,  192, 0,   192, 192, 0,   64,  64,  128, 192, 64,  128,
    64,  192, 128, 192, 192, 128, 0,   0,   64,  128, 0,   64,  0,   128, 64,
    128, 128, 64,  0,   0,   192, 128, 0,   192, 0,   128, 192, 128, 128, 192,
    64,  0,   64,  192, 0,   64,  64,  128, 64,  192, 128, 64,  64,  0,   192,
    192, 0,   192, 64,  128, 192, 192, 128, 192, 0,   64,  64,  128, 64,  64,
    0,   192, 64,  128, 192, 64,  0,   64,  192, 128, 64,  192, 0,   192, 192,
    128, 192, 192, 64,  64,  64,  192, 64,  64,  64,  192, 64,  192, 192, 64,
    64,  64,  192, 192, 64,  192, 64,  192, 192, 192, 192, 192, 32,  0,   0,
    160, 0,   0,   32,  128, 0,   160, 128, 0,   32,  0,   128, 160, 0,   128,
    32,  128, 128, 160, 128, 128, 96,  0,   0,   224, 0,   0,   96,  128, 0,
    224, 128, 0,   96,  0,   128, 224, 0,   128, 96,  128, 128, 224, 128, 128,
    32,  64,  0,   160, 64,  0,   32,  192, 0,   160, 192, 0,   32,  64,  128,
    160, 64,  128, 32,  192, 128, 160, 192, 128, 96,  64,  0,   224, 64,  0,
    96,  192, 0,   224, 192, 0,   96,  64,  128, 224, 64,  128, 96,  192, 128,
    224, 192, 128, 32,  0,   64,  160, 0,   64,  32,  128, 64,  160, 128, 64,
    32,  0,   192, 160, 0,   192, 32,  128, 192, 160, 128, 192, 96,  0,   64,
    224, 0,   64,  96,  128, 64,  224, 128, 64,  96,  0,   192, 224, 0,   192,
    96,  128, 192, 224, 128, 192, 32,  64,  64,  160, 64,  64,  32,  192, 64,
    160, 192, 64,  32,  64,  192, 160, 64,  192, 32,  192, 192, 160, 192, 192,
    96,  64,  64,  224, 64,  64,  96,  192, 64,  224, 192, 64,  96,  64,  192,
    224, 64,  192, 96,  192, 192, 224, 192, 192, 0,   32,  0,   128, 32,  0,
    0,   160, 0,   128, 160, 0,   0,   32,  128, 128, 32,  128, 0,   160, 128,
    128, 160, 128, 64,  32,  0,   192, 32,  0,   64,  160, 0,   192, 160, 0,
    64,  32,  128, 192, 32,  128, 64,  160, 128, 192, 160, 128, 0,   96,  0,
    128, 96,  0,   0,   224, 0,   128, 224, 0,   0,   96,  128, 128, 96,  128,
    0,   224, 128, 128, 224, 128, 64,  96,  0,   192, 96,  0,   64,  224, 0,
    192, 224, 0,   64,  96,  128, 192, 96,  128, 64,  224, 128, 192, 224, 128,
    0,   32,  64,  128, 32,  64,  0,   160, 64,  128, 160, 64,  0,   32,  192,
    128, 32,  192, 0,   160, 192, 128, 160, 192, 64,  32,  64,  192, 32,  64,
    64,  160, 64,  192, 160, 64,  64,  32,  192, 192, 32,  192, 64,  160, 192,
    192, 160, 192, 0,   96,  64,  128, 96,  64,  0,   224, 64,  128, 224, 64,
    0,   96,  192, 128, 96,  192, 0,   224, 192, 128, 224, 192, 64,  96,  64,
    192, 96,  64,  64,  224, 64,  192, 224, 64,  64,  96,  192, 192, 96,  192,
    64,  224, 192, 192, 224, 192, 32,  32,  0,   160, 32,  0,   32,  160, 0,
    160, 160, 0,   32,  32,  128, 160, 32,  128, 32,  160, 128, 160, 160, 128,
    96,  32,  0,   224, 32,  0,   96,  160, 0,   224, 160, 0,   96,  32,  128,
    224, 32,  128, 96,  160, 128, 224, 160, 128, 32,  96,  0,   160, 96,  0,
    32,  224, 0,   160, 224, 0,   32,  96,  128, 160, 96,  128, 32,  224, 128,
    160, 224, 128, 96,  96,  0,   224, 96,  0,   96,  224, 0,   224, 224, 0,
    96,  96,  128, 224, 96,  128, 96,  224, 128, 224, 224, 128, 32,  32,  64,
    160, 32,  64,  32,  160, 64,  160, 160, 64,  32,  32,  192, 160, 32,  192,
    32,  160, 192, 160, 160, 192, 96,  32,  64,  224, 32,  64,  96,  160, 64,
    224, 160, 64,  96,  32,  192, 224, 32,  192, 96,  160, 192, 224, 160, 192,
    32,  96,  64,  160, 96,  64,  32,  224, 64,  160, 224, 64,  32,  96,  192,
    160, 96,  192, 32,  224, 192, 160, 224, 192, 96,  96,  64,  224, 96,  64,
    96,  224, 64,  224, 224, 64,  96,  96,  192, 224, 96,  192, 96,  224, 192,
    224, 224, 192};

StatusOr<std::vector<LabelMapItem>> GetLabelMapIfAny(
    const ModelMetadataExtractor& metadata_extractor,
    const TensorMetadata& tensor_metadata, absl::string_view locale) {
  const std::string labels_filename =
      ModelMetadataExtractor::FindFirstAssociatedFileName(
          tensor_metadata, tflite::AssociatedFileType_TENSOR_AXIS_LABELS);
  if (labels_filename.empty()) {
    return std::vector<LabelMapItem>();
  }
  TFLITE_ASSIGN_OR_RETURN(absl::string_view labels_file,
                   metadata_extractor.GetAssociatedFile(labels_filename));
  const std::string display_names_filename =
      ModelMetadataExtractor::FindFirstAssociatedFileName(
          tensor_metadata, tflite::AssociatedFileType_TENSOR_AXIS_LABELS,
          locale);
  absl::string_view display_names_file = {};
  if (!display_names_filename.empty()) {
    TFLITE_ASSIGN_OR_RETURN(display_names_file, metadata_extractor.GetAssociatedFile(
                                             display_names_filename));
  }
  return BuildLabelMapFromFiles(labels_file, display_names_file);
}

}  // namespace

/* static */
absl::Status ImageSegmenter::SanityCheckOptions(
    const ImageSegmenterOptions& options) {
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
  if (options.output_type() == ImageSegmenterOptions::UNSPECIFIED) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "ImageSegmenterOptions: `output_type` must not be UNSPECIFIED",
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

StatusOr<std::unique_ptr<ImageSegmenter>> ImageSegmenter::CreateFromOptions(
    const ImageSegmenterOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the constructed object.
  auto options_copy = absl::make_unique<ImageSegmenterOptions>(options);

  std::unique_ptr<ImageSegmenter> image_segmenter;
  if (options_copy->has_model_file_with_metadata()) {
    TFLITE_ASSIGN_OR_RETURN(
        image_segmenter,
        TaskAPIFactory::CreateFromExternalFileProto<ImageSegmenter>(
            &options_copy->model_file_with_metadata(), std::move(resolver),
            options_copy->num_threads(), options_copy->compute_settings()));
  } else if (options_copy->base_options().has_model_file()) {
    TFLITE_ASSIGN_OR_RETURN(image_segmenter,
                     TaskAPIFactory::CreateFromBaseOptions<ImageSegmenter>(
                         &options_copy->base_options(), std::move(resolver)));
  } else {
    // Should never happen because of SanityCheckOptions.
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  TFLITE_RETURN_IF_ERROR(image_segmenter->Init(std::move(options_copy)));

  return image_segmenter;
}

absl::Status ImageSegmenter::Init(
    std::unique_ptr<ImageSegmenterOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Perform pre-initialization actions (by default, sets the process engine for
  // image pre-processing to kLibyuv as a sane default).
  TFLITE_RETURN_IF_ERROR(PreInit());

  // Sanity check and set inputs and outputs.
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());
  TFLITE_RETURN_IF_ERROR(CheckAndSetOutputs());

  // Initialize colored_labels_ once and for all.
  TFLITE_RETURN_IF_ERROR(InitColoredLabels());

  return absl::OkStatus();
}

absl::Status ImageSegmenter::PreInit() {
  SetProcessEngine(FrameBufferUtils::ProcessEngine::kLibyuv);
  return absl::OkStatus();
}

absl::Status ImageSegmenter::CheckAndSetOutputs() {
  // First, sanity checks on the model itself.
  const TfLiteEngine::Interpreter* interpreter =
      GetTfLiteEngine()->interpreter();

  // Check the number of output tensors.
  if (TfLiteEngine::OutputCount(interpreter) != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Image segmentation models are expected to have only 1 "
                        "output, found %d",
                        TfLiteEngine::OutputCount(interpreter)),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }
  const TfLiteTensor* output_tensor = TfLiteEngine::GetOutput(interpreter, 0);

  // Check tensor dimensions.
  if (output_tensor->dims->size != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Output tensor is expected to have 4 dimensions, found %d.",
            output_tensor->dims->size),
        TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
  }
  if (output_tensor->dims->data[0] != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected batch size of 1, found %d.",
                        output_tensor->dims->data[0]),
        TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
  }
  output_height_ = output_tensor->dims->data[1];
  output_width_ = output_tensor->dims->data[2];
  output_depth_ = output_tensor->dims->data[3];
  if (output_depth_ > kMaxNumClasses) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected at most %d output classes, found %d",
                        kMaxNumClasses, output_depth_),
        TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
  }

  // Check tensor type.
  if (output_tensor->type != kTfLiteFloat32 &&
      output_tensor->type != kTfLiteUInt8) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Type mismatch for output tensor. Requested one of "
                        "these types: kTfLiteUint8/kTfLiteFloat32, got %s.",
                        TfLiteTypeGetName(output_tensor->type)),
        TfLiteSupportStatus::kInvalidOutputTensorTypeError);
  }
  has_uint8_outputs_ = (output_tensor->type == kTfLiteUInt8);

  // Build label map from metadata, if available.
  const ModelMetadataExtractor* metadata_extractor =
      GetTfLiteEngine()->metadata_extractor();
  const flatbuffers::Vector<flatbuffers::Offset<TensorMetadata>>*
      output_tensor_metadata = metadata_extractor->GetOutputTensorMetadata();
  if (output_tensor_metadata != nullptr) {
    // Check metadata consistency.
    if (output_tensor_metadata->size() != 1) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Mismatch between number of output tensors (1) and "
                          "output tensors metadata (%d).",
                          output_tensor_metadata->size()),
          TfLiteSupportStatus::kMetadataInconsistencyError);
    }
    TFLITE_ASSIGN_OR_RETURN(
        label_map_,
        GetLabelMapIfAny(*metadata_extractor, *output_tensor_metadata->Get(0),
                         options_->display_names_locale()));
  }

  // If label map is still empty, build a default one.
  if (label_map_.empty()) {
    for (int class_index = 0; class_index < output_depth_; ++class_index) {
      label_map_.emplace_back(LabelMapItem{});
    }
  }

  return absl::OkStatus();
}

absl::Status ImageSegmenter::InitColoredLabels() {
  for (int i = 0; i < label_map_.size(); ++i) {
    Segmentation::ColoredLabel colored_label;
    colored_label.set_r(kColorMap[3 * i]);
    colored_label.set_g(kColorMap[3 * i + 1]);
    colored_label.set_b(kColorMap[3 * i + 2]);
    const LabelMapItem& item = label_map_[i];
    if (!item.name.empty()) {
      colored_label.set_class_name(item.name);
    }
    if (!item.display_name.empty()) {
      colored_label.set_display_name(item.display_name);
    }
    colored_labels_.push_back(colored_label);
  }
  return absl::OkStatus();
}

StatusOr<SegmentationResult> ImageSegmenter::Segment(
    const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return InferWithFallback(frame_buffer, roi);
}

StatusOr<SegmentationResult> ImageSegmenter::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const FrameBuffer& frame_buffer, const BoundingBox& /*roi*/) {
  if (output_tensors.size() != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Expected 1 output tensors, found %d",
                        output_tensors.size()));
  }
  const TfLiteTensor* output_tensor = output_tensors[0];

  SegmentationResult result;
  Segmentation* segmentation = result.add_segmentation();
  *segmentation->mutable_colored_labels() = {colored_labels_.begin(),
                                             colored_labels_.end()};

  // The output tensor has orientation `frame_buffer.orientation()`, as it has
  // been produced from the pre-processed frame.
  FrameBuffer::Orientation tensor_orientation = frame_buffer.orientation();
  // The output tensor always has size `output_width_ x output_height_`
  FrameBuffer::Dimension tensor_dimension = {output_width_, output_height_};

  // The masks to produce from the output tensor need to be re-oriented in the
  // unrotated frame of reference coordinates system, i.e. kTopLeft.
  FrameBuffer::Orientation mask_orientation =
      FrameBuffer::Orientation::kTopLeft;
  // They may thus have swapped dimensions compared to the tensor if the
  // rotation is 90° or 270°.
  FrameBuffer::Dimension mask_dimension(tensor_dimension);
  if (RequireDimensionSwap(frame_buffer.orientation(),
                           FrameBuffer::Orientation::kTopLeft)) {
    mask_dimension.Swap();
  }
  segmentation->set_width(mask_dimension.width);
  segmentation->set_height(mask_dimension.height);

  // XY coordinates in the tensor, to be computed from mask_x and mask_y below.
  int tensor_x;
  int tensor_y;

  if (options_->output_type() == ImageSegmenterOptions::CATEGORY_MASK) {
    auto* category_mask = segmentation->mutable_category_mask();
    category_mask->resize(mask_dimension.width * mask_dimension.height);
    int pixel_offset = 0;
    for (int mask_y = 0; mask_y < mask_dimension.height; ++mask_y) {
      for (int mask_x = 0; mask_x < mask_dimension.width; ++mask_x) {
        // Compute the coordinates (tensor_x, tensor_y) in the tensor with
        // tensor_orientation = frame_buffer.orientation() corresponding to the
        // coordinates (mask_x, mask_y) in the mask being filled with
        // mask_orientation = kTopLeft, i.e. the orientation of the unrotated
        // frame of reference.
        OrientCoordinates(/*from_x=*/mask_x,
                          /*from_y=*/mask_y,
                          /*from_orientation=*/mask_orientation,
                          /*to_orientation=*/tensor_orientation,
                          /*from_dimension=*/mask_dimension,
                          /*to_x=*/&tensor_x,
                          /*to_y=*/&tensor_y);
        int class_index = 0;
        float max_confidence = 0.0f;
        for (int d = 0; d < output_depth_; ++d) {
          TFLITE_ASSIGN_OR_RETURN(
              const float confidence,
              GetOutputConfidence(*output_tensor, tensor_x, tensor_y, d));
          if (confidence > max_confidence) {
            class_index = d;
            max_confidence = confidence;
          }
        }
        (*category_mask)[pixel_offset++] = static_cast<char>(class_index);
      }
    }
  } else if (options_->output_type() ==
             ImageSegmenterOptions::CONFIDENCE_MASK) {
    auto* confidence_masks = segmentation->mutable_confidence_masks();
    for (int d = 0; d < output_depth_; ++d) {
      confidence_masks->add_confidence_mask();
    }
    for (int mask_y = 0; mask_y < segmentation->height(); ++mask_y) {
      for (int mask_x = 0; mask_x < segmentation->width(); ++mask_x) {
        // See above.
        OrientCoordinates(/*from_x=*/mask_x,
                          /*from_y=*/mask_y,
                          /*from_orientation=*/mask_orientation,
                          /*to_orientation=*/tensor_orientation,
                          /*from_dimension=*/mask_dimension,
                          /*to_x=*/&tensor_x,
                          /*to_y=*/&tensor_y);
        for (int d = 0; d < output_depth_; ++d) {
          TFLITE_ASSIGN_OR_RETURN(
              float confidence,
              GetOutputConfidence(*output_tensor, tensor_x, tensor_y, d));
          confidence_masks->mutable_confidence_mask(d)->add_value(confidence);
        }
      }
    }
  }

  return result;
}

StatusOr<float> ImageSegmenter::GetOutputConfidence(
    const TfLiteTensor& output_tensor, int x, int y, int depth) {
  int index = output_width_ * output_depth_ * y + output_depth_ * x + depth;
  if (has_uint8_outputs_) {
    TFLITE_ASSIGN_OR_RETURN(const uint8* data,
                     AssertAndReturnTypedTensor<uint8>(&output_tensor));
    return output_tensor.params.scale *
           (static_cast<int>(data[index]) - output_tensor.params.zero_point);
  } else {
    TFLITE_ASSIGN_OR_RETURN(const float* data,
                     AssertAndReturnTypedTensor<float>(&output_tensor));
    return data[index];
  }
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
