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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_VISION_CORE_BASE_VISION_TASK_API_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_VISION_CORE_BASE_VISION_TASK_API_H_

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_tensor_specs.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace vision {

// Base class providing common logic for vision models.
template <class OutputType>
class BaseVisionTaskApi
    : public tflite::task::core::
          BaseTaskApi<OutputType, const FrameBuffer&, const BoundingBox&> {
 public:
  explicit BaseVisionTaskApi(std::unique_ptr<core::TfLiteEngine> engine)
      : tflite::task::core::BaseTaskApi<OutputType,
                                        const FrameBuffer&,
                                        const BoundingBox&>(std::move(engine)) {
  }
  // BaseVisionTaskApi is neither copyable nor movable.
  BaseVisionTaskApi(const BaseVisionTaskApi&) = delete;
  BaseVisionTaskApi& operator=(const BaseVisionTaskApi&) = delete;

  // Number of bytes required for 8-bit per pixel RGB color space.
  static constexpr int kRgbPixelBytes = 3;

  // Sets the ProcessEngine used for image pre-processing. Must be called before
  // any inference is performed. Can be called between inferences to override
  // the current process engine.
  void SetProcessEngine(const FrameBufferUtils::ProcessEngine& process_engine) {
    frame_buffer_utils_ = FrameBufferUtils::Create(process_engine);
  }

 protected:
  using tflite::task::core::
      BaseTaskApi<OutputType, const FrameBuffer&, const BoundingBox&>::engine_;

  // Checks input tensor and metadata (if any) are valid, or return an error
  // otherwise. This must be called once at initialization time, before running
  // inference, as it is a prerequisite for `Preprocess`.
  // Note: the underlying interpreter and metadata extractor are assumed to be
  // already successfully initialized before calling this method.
  virtual absl::Status CheckAndSetInputs() {
    ASSIGN_OR_RETURN(
        ImageTensorSpecs input_specs,
        BuildInputImageTensorSpecs(*engine_->interpreter(),
                                   *engine_->metadata_extractor()));

    if (input_specs.color_space != tflite::ColorSpaceType_RGB) {
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kUnimplemented,
          "BaseVisionTaskApi only supports RGB color space for now.");
    }

    input_specs_ = absl::make_unique<ImageTensorSpecs>(input_specs);

    return absl::OkStatus();
  }

  // Performs image preprocessing on the input frame buffer over the region of
  // interest so that it fits model requirements (e.g. upright 224x224 RGB) and
  // populate the corresponding input tensor. This is performed by (in this
  // order):
  // - cropping the frame buffer to the region of interest (which, in most
  //   cases, just covers the entire input image),
  // - resizing it (with bilinear interpolation, aspect-ratio *not* preserved)
  //   to the dimensions of the model input tensor,
  // - converting it to the colorspace of the input tensor (i.e. RGB, which is
  //   the only supported colorspace for now),
  // - rotating it according to its `Orientation` so that inference is performed
  //   on an "upright" image.
  //
  // IMPORTANT: as a consequence of cropping occurring first, the provided
  // region of interest is expressed in the unrotated frame of reference
  // coordinates system, i.e. in `[0, frame_buffer.width) x [0,
  // frame_buffer.height)`, which are the dimensions of the underlying
  // `frame_buffer` data before any `Orientation` flag gets applied. Also, the
  // region of interest is not clamped, so this method will return a non-ok
  // status if the region is out of these bounds.
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          const FrameBuffer& frame_buffer,
                          const BoundingBox& roi) override {
    if (input_specs_ == nullptr) {
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kInternal,
          "Uninitialized input tensor specs: CheckAndSetInputs must be called "
          "at initialization time.");
    }

    if (frame_buffer_utils_ == nullptr) {
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kInternal,
          "Uninitialized frame buffer utils: SetProcessEngine must be called "
          "at initialization time.");
    }

    if (input_tensors.size() != 1) {
      return tflite::support::CreateStatusWithPayload(
          absl::StatusCode::kInternal, "A single input tensor is expected.");
    }

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
      // `CheckAndSetInputs`.
      FrameBuffer::Dimension to_buffer_dimension = {input_specs_->image_width,
                                                    input_specs_->image_height};
      input_data_byte_size =
          GetBufferByteSize(to_buffer_dimension, FrameBuffer::Format::kRGB);
      preprocessed_data.resize(input_data_byte_size / sizeof(uint8), 0);
      input_data = preprocessed_data.data();

      FrameBuffer::Plane preprocessed_plane = {
          /*buffer=*/preprocessed_data.data(),
          /*stride=*/{input_specs_->image_width * kRgbPixelBytes,
                      kRgbPixelBytes}};
      preprocessed_frame_buffer = FrameBuffer::Create(
          {preprocessed_plane}, to_buffer_dimension, FrameBuffer::Format::kRGB,
          FrameBuffer::Orientation::kTopLeft);

      RETURN_IF_ERROR(frame_buffer_utils_->Preprocess(
          frame_buffer, roi, preprocessed_frame_buffer.get()));
    } else {
      // Input frame buffer already targets model requirements: skip image
      // preprocessing. For RGB, the data is always stored in a single plane.
      input_data = frame_buffer.plane(0).buffer;
      input_data_byte_size = frame_buffer.plane(0).stride.row_stride_bytes *
                             frame_buffer.dimension().height;
    }

    // Then normalize pixel data (if needed) and populate the input tensor.
    switch (input_specs_->tensor_type) {
      case kTfLiteUInt8:
        if (input_tensors[0]->bytes != input_data_byte_size) {
          return tflite::support::CreateStatusWithPayload(
              absl::StatusCode::kInternal,
              "Size mismatch or unsupported padding bytes between pixel data "
              "and input tensor.");
        }
        // No normalization required: directly populate data.
        tflite::task::core::PopulateTensor(
            input_data, input_data_byte_size / sizeof(uint8), input_tensors[0]);
        break;
      case kTfLiteFloat32: {
        if (input_tensors[0]->bytes / sizeof(float) !=
            input_data_byte_size / sizeof(uint8)) {
          return tflite::support::CreateStatusWithPayload(
              absl::StatusCode::kInternal,
              "Size mismatch or unsupported padding bytes between pixel data "
              "and input tensor.");
        }
        // Normalize and populate.
        float* normalized_input_data =
            tflite::task::core::AssertAndReturnTypedTensor<float>(
                input_tensors[0]);
        const tflite::task::vision::NormalizationOptions&
            normalization_options = input_specs_->normalization_options.value();
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

  // Utils for input image preprocessing (resizing, colorspace conversion, etc).
  std::unique_ptr<FrameBufferUtils> frame_buffer_utils_;

  // Parameters related to the input tensor which represents an image.
  std::unique_ptr<ImageTensorSpecs> input_specs_;

 private:
  // Returns false if image preprocessing could be skipped, true otherwise.
  bool IsImagePreprocessingNeeded(const FrameBuffer& frame_buffer,
                                  const BoundingBox& roi) {
    // Is crop required?
    if (roi.origin_x() != 0 || roi.origin_y() != 0 ||
        roi.width() != frame_buffer.dimension().width ||
        roi.height() != frame_buffer.dimension().height) {
      return true;
    }

    // Are image transformations required?
    if (frame_buffer.orientation() != FrameBuffer::Orientation::kTopLeft ||
        frame_buffer.format() != FrameBuffer::Format::kRGB ||
        frame_buffer.dimension().width != input_specs_->image_width ||
        frame_buffer.dimension().height != input_specs_->image_height) {
      return true;
    }

    return false;
  }
};

}  // namespace vision
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_VISION_CORE_BASE_VISION_TASK_API_H_
