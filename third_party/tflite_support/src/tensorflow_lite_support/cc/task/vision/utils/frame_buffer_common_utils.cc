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

#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::tflite::support::StatusOr;

constexpr int kRgbaChannels = 4;
constexpr int kRgbChannels = 3;
constexpr int kGrayChannel = 1;

// Creates a FrameBuffer from one plane raw NV21/NV12 buffer and passing
// arguments.
StatusOr<std::unique_ptr<FrameBuffer>> CreateFromOnePlaneNVRawBuffer(
    const uint8* input, FrameBuffer::Dimension dimension,
    FrameBuffer::Format format, FrameBuffer::Orientation orientation,
    const absl::Time timestamp) {
  FrameBuffer::Plane input_plane = {/*buffer=*/input,
                                    /*stride=*/{dimension.width, kGrayChannel}};
  return FrameBuffer::Create({input_plane}, dimension, format, orientation,
                             timestamp);
}

// Indicates whether the given buffers have the same dimensions.
bool AreBufferDimsEqual(const FrameBuffer& buffer1,
                        const FrameBuffer& buffer2) {
  return buffer1.dimension() == buffer2.dimension();
}

// Indicates whether the given buffers formats are compatible. Same formats are
// compatible and all YUV family formats (e.g. NV21, NV12, YV12, YV21, etc) are
// compatible.
bool AreBufferFormatsCompatible(const FrameBuffer& buffer1,
                                const FrameBuffer& buffer2) {
  switch (buffer1.format()) {
    case FrameBuffer::Format::kRGBA:
    case FrameBuffer::Format::kRGB:
      return (buffer2.format() == FrameBuffer::Format::kRGBA ||
              buffer2.format() == FrameBuffer::Format::kRGB);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return (buffer2.format() == FrameBuffer::Format::kNV12 ||
              buffer2.format() == FrameBuffer::Format::kNV21 ||
              buffer2.format() == FrameBuffer::Format::kYV12 ||
              buffer2.format() == FrameBuffer::Format::kYV21);
    case FrameBuffer::Format::kGRAY:
    default:
      return buffer1.format() == buffer2.format();
  }
}

}  // namespace

// Miscellaneous Methods
// -----------------------------------------------------------------
int GetFrameBufferByteSize(FrameBuffer::Dimension dimension,
                           FrameBuffer::Format format) {
  switch (format) {
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return /*y plane*/ dimension.Size() +
             /*uv plane*/ (dimension.width + 1) / 2 * (dimension.height + 1) /
                 2 * 2;
    case FrameBuffer::Format::kRGB:
      return dimension.Size() * kRgbPixelBytes;
    case FrameBuffer::Format::kRGBA:
      return dimension.Size() * kRgbaPixelBytes;
    case FrameBuffer::Format::kGRAY:
      return dimension.Size();
    default:
      return 0;
  }
}

StatusOr<int> GetPixelStrides(FrameBuffer::Format format) {
  switch (format) {
    case FrameBuffer::Format::kGRAY:
      return kGrayPixelBytes;
    case FrameBuffer::Format::kRGB:
      return kRgbPixelBytes;
    case FrameBuffer::Format::kRGBA:
      return kRgbaPixelBytes;
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "GetPixelStrides does not support format: %i.", format));
  }
}

StatusOr<const uint8*> GetUvRawBuffer(const FrameBuffer& buffer) {
  if (buffer.format() != FrameBuffer::Format::kNV12 &&
      buffer.format() != FrameBuffer::Format::kNV21) {
    return absl::InvalidArgumentError(
        "Only support getting biplanar UV buffer from NV12/NV21 frame buffer.");
  }
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData yuv_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  const uint8* uv_buffer = buffer.format() == FrameBuffer::Format::kNV12
                               ? yuv_data.u_buffer
                               : yuv_data.v_buffer;
  return uv_buffer;
}

StatusOr<FrameBuffer::Dimension> GetUvPlaneDimension(
    FrameBuffer::Dimension dimension, FrameBuffer::Format format) {
  if (dimension.width <= 0 || dimension.height <= 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid input dimension: {%d, %d}.", dimension.width,
                        dimension.height));
  }
  switch (format) {
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return FrameBuffer::Dimension{(dimension.width + 1) / 2,
                                    (dimension.height + 1) / 2};
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Input format is not YUV-like: %i.", format));
  }
}

FrameBuffer::Dimension GetCropDimension(int x0, int x1, int y0, int y1) {
  return {x1 - x0 + 1, y1 - y0 + 1};
}

// Validation Methods
// -----------------------------------------------------------------

absl::Status ValidateBufferPlaneMetadata(const FrameBuffer& buffer) {
  if (buffer.plane_count() < 1) {
    return absl::InvalidArgumentError(
        "There must be at least 1 plane specified.");
  }

  for (int i = 0; i < buffer.plane_count(); i++) {
    if (buffer.plane(i).stride.row_stride_bytes == 0 ||
        buffer.plane(i).stride.pixel_stride_bytes == 0) {
      return absl::InvalidArgumentError("Invalid stride information.");
    }
  }

  return absl::OkStatus();
}

absl::Status ValidateBufferFormat(const FrameBuffer& buffer) {
  switch (buffer.format()) {
    case FrameBuffer::Format::kGRAY:
    case FrameBuffer::Format::kRGB:
    case FrameBuffer::Format::kRGBA:
      if (buffer.plane_count() == 1) return absl::OkStatus();
      return absl::InvalidArgumentError(
          "Plane count must be 1 for grayscale and RGB[a] buffers.");
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kYV21:
    case FrameBuffer::Format::kYV12:
      return absl::OkStatus();
    default:
      return absl::InternalError(
          absl::StrFormat("Unsupported buffer format: %i.", buffer.format()));
  }
}

absl::Status ValidateBufferFormats(const FrameBuffer& buffer1,
                                   const FrameBuffer& buffer2) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormat(buffer1));
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormat(buffer2));
  return absl::OkStatus();
}

absl::Status ValidateResizeBufferInputs(const FrameBuffer& buffer,
                                        const FrameBuffer& output_buffer) {
  bool valid_format = false;
  switch (buffer.format()) {
    case FrameBuffer::Format::kGRAY:
    case FrameBuffer::Format::kRGB:
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      valid_format = (buffer.format() == output_buffer.format());
      break;
    case FrameBuffer::Format::kRGBA:
      valid_format = (output_buffer.format() == FrameBuffer::Format::kRGBA ||
                      output_buffer.format() == FrameBuffer::Format::kRGB);
      break;
    default:
      return absl::InternalError(
          absl::StrFormat("Unsupported buffer format: %i.", buffer.format()));
  }
  if (!valid_format) {
    return absl::InvalidArgumentError(
        "Input and output buffer formats must match.");
  }
  return ValidateBufferFormats(buffer, output_buffer);
}

absl::Status ValidateRotateBufferInputs(const FrameBuffer& buffer,
                                        const FrameBuffer& output_buffer,
                                        int angle_deg) {
  if (!AreBufferFormatsCompatible(buffer, output_buffer)) {
    return absl::InvalidArgumentError(
        "Input and output buffer formats must match.");
  }

  const bool is_dimension_change = (angle_deg / 90) % 2 == 1;
  const bool are_dimensions_rotated =
      (buffer.dimension().width == output_buffer.dimension().height) &&
      (buffer.dimension().height == output_buffer.dimension().width);
  const bool are_dimensions_equal =
      buffer.dimension() == output_buffer.dimension();

  if (angle_deg >= 360 || angle_deg <= 0 || angle_deg % 90 != 0) {
    return absl::InvalidArgumentError(
        "Rotation angle must be between 0 and 360, in multiples of 90 "
        "degrees.");
  } else if ((is_dimension_change && !are_dimensions_rotated) ||
             (!is_dimension_change && !are_dimensions_equal)) {
    return absl::InvalidArgumentError(
        "Output buffer has invalid dimensions for rotation.");
  }
  return absl::OkStatus();
}

absl::Status ValidateCropBufferInputs(const FrameBuffer& buffer,
                                      const FrameBuffer& output_buffer, int x0,
                                      int y0, int x1, int y1) {
  if (!AreBufferFormatsCompatible(buffer, output_buffer)) {
    return absl::InvalidArgumentError(
        "Input and output buffer formats must match.");
  }

  bool is_buffer_size_valid =
      ((x1 < buffer.dimension().width) && y1 < buffer.dimension().height);
  bool are_points_valid = (x0 >= 0) && (y0 >= 0) && (x1 >= x0) && (y1 >= y0);

  if (!is_buffer_size_valid || !are_points_valid) {
    return absl::InvalidArgumentError("Invalid crop coordinates.");
  }
  return absl::OkStatus();
}

absl::Status ValidateFlipBufferInputs(const FrameBuffer& buffer,
                                      const FrameBuffer& output_buffer) {
  if (!AreBufferFormatsCompatible(buffer, output_buffer)) {
    return absl::InvalidArgumentError(
        "Input and output buffer formats must match.");
  }
  return AreBufferDimsEqual(buffer, output_buffer)
             ? absl::OkStatus()
             : absl::InvalidArgumentError(
                   "Input and output buffers must have the same dimensions.");
}

absl::Status ValidateConvertFormats(FrameBuffer::Format from_format,
                                    FrameBuffer::Format to_format) {
  if (from_format == to_format) {
    return absl::InvalidArgumentError("Formats must be different.");
  }

  switch (from_format) {
    case FrameBuffer::Format::kGRAY:
      return absl::InvalidArgumentError(
          "Grayscale format does not convert to other formats.");
    case FrameBuffer::Format::kRGB:
    case FrameBuffer::Format::kRGBA:
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return absl::OkStatus();
    default:
      return absl::InternalError(
          absl::StrFormat("Unsupported buffer format: %i.", from_format));
  }
}

// Creation Methods
// -----------------------------------------------------------------

// Creates a FrameBuffer from raw RGBA buffer and passing arguments.
std::unique_ptr<FrameBuffer> CreateFromRgbaRawBuffer(
    const uint8* input, FrameBuffer::Dimension dimension,
    FrameBuffer::Orientation orientation, const absl::Time timestamp,
    FrameBuffer::Stride stride) {
  if (stride == kDefaultStride) {
    stride.row_stride_bytes = dimension.width * kRgbaChannels;
    stride.pixel_stride_bytes = kRgbaChannels;
  }
  FrameBuffer::Plane input_plane = {/*buffer=*/input,
                                    /*stride=*/stride};
  return FrameBuffer::Create({input_plane}, dimension,
                             FrameBuffer::Format::kRGBA, orientation,
                             timestamp);
}

// Creates a FrameBuffer from raw RGB buffer and passing arguments.
std::unique_ptr<FrameBuffer> CreateFromRgbRawBuffer(
    const uint8* input, FrameBuffer::Dimension dimension,
    FrameBuffer::Orientation orientation, const absl::Time timestamp,
    FrameBuffer::Stride stride) {
  if (stride == kDefaultStride) {
    stride.row_stride_bytes = dimension.width * kRgbChannels;
    stride.pixel_stride_bytes = kRgbChannels;
  }
  FrameBuffer::Plane input_plane = {/*buffer=*/input,
                                    /*stride=*/stride};
  return FrameBuffer::Create({input_plane}, dimension,
                             FrameBuffer::Format::kRGB, orientation, timestamp);
}

// Creates a FrameBuffer from raw grayscale buffer and passing arguments.
std::unique_ptr<FrameBuffer> CreateFromGrayRawBuffer(
    const uint8* input, FrameBuffer::Dimension dimension,
    FrameBuffer::Orientation orientation, const absl::Time timestamp,
    FrameBuffer::Stride stride) {
  if (stride == kDefaultStride) {
    stride.row_stride_bytes = dimension.width * kGrayChannel;
    stride.pixel_stride_bytes = kGrayChannel;
  }
  FrameBuffer::Plane input_plane = {/*buffer=*/input,
                                    /*stride=*/stride};
  return FrameBuffer::Create({input_plane}, dimension,
                             FrameBuffer::Format::kGRAY, orientation,
                             timestamp);
}

// Creates a FrameBuffer from raw YUV buffer and passing arguments.
StatusOr<std::unique_ptr<FrameBuffer>> CreateFromYuvRawBuffer(
    const uint8* y_plane, const uint8* u_plane, const uint8* v_plane,
    FrameBuffer::Format format, FrameBuffer::Dimension dimension,
    int row_stride_y, int row_stride_uv, int pixel_stride_uv,
    FrameBuffer::Orientation orientation, const absl::Time timestamp) {
  const int pixel_stride_y = 1;
  std::vector<FrameBuffer::Plane> planes;
  if (format == FrameBuffer::Format::kNV21 ||
      format == FrameBuffer::Format::kYV12) {
    planes = {{y_plane, /*stride=*/{row_stride_y, pixel_stride_y}},
              {v_plane, /*stride=*/{row_stride_uv, pixel_stride_uv}},
              {u_plane, /*stride=*/{row_stride_uv, pixel_stride_uv}}};
  } else if (format == FrameBuffer::Format::kNV12 ||
             format == FrameBuffer::Format::kYV21) {
    planes = {{y_plane, /*stride=*/{row_stride_y, pixel_stride_y}},
              {u_plane, /*stride=*/{row_stride_uv, pixel_stride_uv}},
              {v_plane, /*stride=*/{row_stride_uv, pixel_stride_uv}}};
  } else {
    return absl::InvalidArgumentError(
        absl::StrFormat("Input format is not YUV-like: %i.", format));
  }
  return FrameBuffer::Create(planes, dimension, format, orientation, timestamp);
}

StatusOr<std::unique_ptr<FrameBuffer>> CreateFromRawBuffer(
    const uint8* buffer, FrameBuffer::Dimension dimension,
    const FrameBuffer::Format target_format,
    FrameBuffer::Orientation orientation, absl::Time timestamp) {
  switch (target_format) {
    case FrameBuffer::Format::kNV12:
      return CreateFromOnePlaneNVRawBuffer(buffer, dimension, target_format,
                                           orientation, timestamp);
    case FrameBuffer::Format::kNV21:
      return CreateFromOnePlaneNVRawBuffer(buffer, dimension, target_format,
                                           orientation, timestamp);
    case FrameBuffer::Format::kYV12: {
      TFLITE_ASSIGN_OR_RETURN(const FrameBuffer::Dimension uv_dimension,
                       GetUvPlaneDimension(dimension, target_format));
      return CreateFromYuvRawBuffer(
          /*y_plane=*/buffer,
          /*u_plane=*/buffer + dimension.Size() + uv_dimension.Size(),
          /*v_plane=*/buffer + dimension.Size(), target_format, dimension,
          /*row_stride_y=*/dimension.width, uv_dimension.width,
          /*pixel_stride_uv=*/1, orientation, timestamp);
    }
    case FrameBuffer::Format::kYV21: {
      TFLITE_ASSIGN_OR_RETURN(const FrameBuffer::Dimension uv_dimension,
                       GetUvPlaneDimension(dimension, target_format));
      return CreateFromYuvRawBuffer(
          /*y_plane=*/buffer, /*u_plane=*/buffer + dimension.Size(),
          /*v_plane=*/buffer + dimension.Size() + uv_dimension.Size(),
          target_format, dimension, /*row_stride_y=*/dimension.width,
          uv_dimension.width,
          /*pixel_stride_uv=*/1, orientation, timestamp);
    }
    case FrameBuffer::Format::kRGBA:
      return CreateFromRgbaRawBuffer(buffer, dimension, orientation, timestamp);
    case FrameBuffer::Format::kRGB:
      return CreateFromRgbRawBuffer(buffer, dimension, orientation, timestamp);
    case FrameBuffer::Format::kGRAY:
      return CreateFromGrayRawBuffer(buffer, dimension, orientation, timestamp);
    default:

      return absl::InternalError(
          absl::StrFormat("Unsupported buffer format: %i.", target_format));
  }
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
