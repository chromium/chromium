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

#include "tensorflow_lite_support/cc/task/processor/image_preprocessor.h"

#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_tensor_specs.h"

namespace tflite {
namespace task {
namespace processor {

namespace {
// Number of bytes required for 8-bit per pixel RGB color space.
static constexpr int kRgbPixelBytes = 3;

using ::tflite::task::vision::BoundingBox;
using ::tflite::task::vision::FrameBuffer;
}  // namespace

/* static */
tflite::support::StatusOr<std::unique_ptr<ImagePreprocessor>>
ImagePreprocessor::Create(
    core::TfLiteEngine* engine, const std::initializer_list<int> input_indices,
    const vision::FrameBufferUtils::ProcessEngine& process_engine) {
  TFLITE_ASSIGN_OR_RETURN(auto processor,
                   Processor::Create<ImagePreprocessor>(
                       /* num_expected_tensors = */ 1, engine, input_indices,
                       /* requires_metadata = */ false));

  TFLITE_RETURN_IF_ERROR(processor->Init(process_engine));
  return processor;
}

// Returns false if image preprocessing could be skipped, true otherwise.
bool ImagePreprocessor::IsImagePreprocessingNeeded(
    const FrameBuffer& frame_buffer, const BoundingBox& roi) {
  // Is crop required?
  if (roi.origin_x() != 0 || roi.origin_y() != 0 ||
      roi.width() != frame_buffer.dimension().width ||
      roi.height() != frame_buffer.dimension().height) {
    return true;
  }

  // Are image transformations required?
  if (frame_buffer.orientation() != FrameBuffer::Orientation::kTopLeft ||
      frame_buffer.format() != FrameBuffer::Format::kRGB ||
      frame_buffer.dimension().width != input_specs_.image_width ||
      frame_buffer.dimension().height != input_specs_.image_height) {
    return true;
  }

  return false;
}

absl::Status ImagePreprocessor::Init(
    const vision::FrameBufferUtils::ProcessEngine& process_engine) {
  frame_buffer_utils_ = vision::FrameBufferUtils::Create(process_engine);

  TFLITE_ASSIGN_OR_RETURN(input_specs_, vision::BuildInputImageTensorSpecs(
                                     *engine_->interpreter(),
                                     *engine_->metadata_extractor()));

  if (input_specs_.color_space != tflite::ColorSpaceType_RGB) {
    return tflite::support::CreateStatusWithPayload(
        absl::StatusCode::kUnimplemented,
        "ImagePreprocessor only supports RGB color space for now.");
  }

  // Determine if the input shape is resizable.
  const TfLiteIntArray* dims_signature = GetTensor()->dims_signature;

  // Some fixed-shape models do not have dims_signature.
  if (dims_signature != nullptr && dims_signature->size > 2) {
    // Only the HxW dimensions support mutability.
    is_height_mutable_ = dims_signature->data[1] == -1;
    is_width_mutable_ = dims_signature->data[2] == -1;
  }
  return absl::OkStatus();
}

absl::Status ImagePreprocessor::Preprocess(const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return Preprocess(frame_buffer, roi);
}

absl::Status ImagePreprocessor::Preprocess(const FrameBuffer& frame_buffer,
                                           const BoundingBox& roi) {
  // Input data to be normalized (if needed) and used for inference. In most
  // cases, this is the result of image preprocessing. In case no image
  // preprocessing is needed (see below), this points to the input frame
  // buffer raw data.
  const uint8* input_data;
  size_t input_data_byte_size;

  // Optional buffers in case image preprocessing is needed.
  std::unique_ptr<FrameBuffer> preprocessed_frame_buffer;
  std::vector<uint8> preprocessed_data;

  if (IsImagePreprocessingNeeded(frame_buffer, roi)) {
    // Preprocess input image to fit model requirements.
    // For now RGB is the only color space supported, which is ensured by
    // `InitInternal`.
    input_specs_.image_width =
        is_width_mutable_ ? roi.width() : input_specs_.image_width;
    input_specs_.image_height =
        is_height_mutable_ ? roi.height() : input_specs_.image_height;

    FrameBuffer::Dimension to_buffer_dimension = {input_specs_.image_width,
                                                  input_specs_.image_height};
    input_data_byte_size =
        GetBufferByteSize(to_buffer_dimension, FrameBuffer::Format::kRGB);
    preprocessed_data.resize(input_data_byte_size / sizeof(uint8), 0);
    input_data = preprocessed_data.data();

    FrameBuffer::Plane preprocessed_plane = {
        /*buffer=*/preprocessed_data.data(),
        /*stride=*/{input_specs_.image_width * kRgbPixelBytes, kRgbPixelBytes}};
    preprocessed_frame_buffer = FrameBuffer::Create(
        {preprocessed_plane}, to_buffer_dimension, FrameBuffer::Format::kRGB,
        FrameBuffer::Orientation::kTopLeft);

    TFLITE_RETURN_IF_ERROR(frame_buffer_utils_->Preprocess(
        frame_buffer, roi, preprocessed_frame_buffer.get()));
  } else {
    // Input frame buffer already targets model requirements: skip image
    // preprocessing. For RGB, the data is always stored in a single plane.
    input_data = frame_buffer.plane(0).buffer;
    input_data_byte_size = frame_buffer.plane(0).stride.row_stride_bytes *
                           frame_buffer.dimension().height;
  }

  // If dynamic, it will re-dim the entire graph as per the input.
  if (is_height_mutable_ || is_width_mutable_) {
    engine_->interpreter()->ResizeInputTensorStrict(
        0, {GetTensor()->dims->data[0], input_specs_.image_height,
            input_specs_.image_width, GetTensor()->dims->data[3]});

    engine_->interpreter()->AllocateTensors();
  }
  // Then normalize pixel data (if needed) and populate the input tensor.
  switch (input_specs_.tensor_type) {
    case kTfLiteUInt8:
      if (GetTensor()->bytes != input_data_byte_size) {
        return tflite::support::CreateStatusWithPayload(
            absl::StatusCode::kInternal,
            "Size mismatch or unsupported padding bytes between pixel data "
            "and input tensor.");
      }
      // No normalization required: directly populate data.
      TFLITE_RETURN_IF_ERROR(tflite::task::core::PopulateTensor(
          input_data, input_data_byte_size / sizeof(uint8), GetTensor()));
      break;
    case kTfLiteFloat32: {
      if (GetTensor()->bytes / sizeof(float) !=
          input_data_byte_size / sizeof(uint8)) {
        return tflite::support::CreateStatusWithPayload(
            absl::StatusCode::kInternal,
            "Size mismatch or unsupported padding bytes between pixel data "
            "and input tensor.");
      }
      // Normalize and populate.
      TFLITE_ASSIGN_OR_RETURN(
          float* normalized_input_data,
          tflite::task::core::AssertAndReturnTypedTensor<float>(GetTensor()));
      const tflite::task::vision::NormalizationOptions& normalization_options =
          input_specs_.normalization_options.value();
      for (int i = 0; i < normalization_options.num_values; i++) {
        if (std::abs(normalization_options.std_values[i]) <
            std::numeric_limits<float>::epsilon()) {
          return tflite::support::CreateStatusWithPayload(
              absl::StatusCode::kInternal,
              "NormalizationOptions.std_values can't be 0. Please check if the "
              "tensor metadata has been populated correctly.");
        }
      }
      if (normalization_options.num_values == 1) {
        float mean_value = normalization_options.mean_values[0];
        float inv_std_value = (1.0f / normalization_options.std_values[0]);
        for (size_t i = 0; i < input_data_byte_size / sizeof(uint8);
             i++, input_data++, normalized_input_data++) {
          *normalized_input_data =
              inv_std_value * (static_cast<float>(*input_data) - mean_value);
        }
      } else {
        std::array<float, 3> inv_std_values = {
            1.0f / normalization_options.std_values[0],
            1.0f / normalization_options.std_values[1],
            1.0f / normalization_options.std_values[2]};
        for (size_t i = 0; i < input_data_byte_size / sizeof(uint8);
             i++, input_data++, normalized_input_data++) {
          *normalized_input_data = inv_std_values[i % 3] *
                                   (static_cast<float>(*input_data) -
                                    normalization_options.mean_values[i % 3]);
        }
      }
      break;
    }
    case kTfLiteInt8:
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kUnimplemented,
          "kTfLiteInt8 input type is not implemented yet.");
    default:
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kInternal, "Unexpected input tensor type.");
  }

  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
