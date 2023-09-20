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

#include "tensorflow_lite_support/cc/task/vision/utils/libyuv_frame_buffer_utils.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "libyuv.h"  // from @libyuv
#include "libyuv/convert_argb.h"  // from @libyuv
#include "libyuv/scale.h"  // from @libyuv
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"

namespace tflite {
namespace task {
namespace vision {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::TfLiteSupportStatus;

namespace {

// Converts NV12 `buffer` to the `output_buffer` of the target color space.
// Supported output format includes RGB24 and YV21.
absl::Status ConvertFromNv12(const FrameBuffer& buffer,
                             FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData yuv_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  switch (output_buffer->format()) {
    case FrameBuffer::Format::kRGB: {
      // The RAW format of Libyuv represents the 8-bit interleaved RGB format in
      // the big endian style with R being the first byte in memory.
      int ret = libyuv::NV12ToRAW(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV12ToRAW operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kRGBA: {
      // The libyuv ABGR format is interleaved RGBA format in memory.
      int ret = libyuv::NV12ToABGR(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV12ToABGR operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::NV12ToI420(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride, const_cast<uint8_t*>(output_data.y_buffer),
          output_data.y_row_stride, const_cast<uint8_t*>(output_data.u_buffer),
          output_data.uv_row_stride, const_cast<uint8_t*>(output_data.v_buffer),
          output_data.uv_row_stride, output_buffer->dimension().width,
          output_buffer->dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV12ToI420 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_data.y_buffer),
                        output_data.y_row_stride, buffer.dimension().width,
                        buffer.dimension().height);
      TFLITE_ASSIGN_OR_RETURN(
          const FrameBuffer::Dimension uv_plane_dimension,
          GetUvPlaneDimension(buffer.dimension(), buffer.format()));
      libyuv::SwapUVPlane(yuv_data.u_buffer, yuv_data.uv_row_stride,
                          const_cast<uint8*>(output_data.v_buffer),
                          output_data.uv_row_stride, uv_plane_dimension.width,
                          uv_plane_dimension.height);
      break;
    }
    case FrameBuffer::Format::kGRAY: {
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_buffer->plane(0).buffer),
                        output_buffer->plane(0).stride.row_stride_bytes,
                        output_buffer->dimension().width,
                        output_buffer->dimension().height);
      break;
    }
    default:
      return absl::InternalError(absl::StrFormat("Format %i is not supported.",
                                                 output_buffer->format()));
  }
  return absl::OkStatus();
}

// Converts NV21 `buffer` into the `output_buffer` of the target color space.
// Supported output format includes RGB24 and YV21.
absl::Status ConvertFromNv21(const FrameBuffer& buffer,
                             FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData yuv_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  switch (output_buffer->format()) {
    case FrameBuffer::Format::kRGB: {
      // The RAW format of Libyuv represents the 8-bit interleaved RGB format in
      // the big endian style with R being the first byte in memory.
      int ret = libyuv::NV21ToRAW(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.v_buffer,
          yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV21ToRAW operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kRGBA: {
      // The libyuv ABGR format is interleaved RGBA format in memory.
      int ret = libyuv::NV21ToABGR(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.v_buffer,
          yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV21ToABGR operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::NV21ToI420(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.v_buffer,
          yuv_data.uv_row_stride, const_cast<uint8_t*>(output_data.y_buffer),
          output_data.y_row_stride, const_cast<uint8_t*>(output_data.u_buffer),
          output_data.uv_row_stride, const_cast<uint8_t*>(output_data.v_buffer),
          output_data.uv_row_stride, output_buffer->dimension().width,
          output_buffer->dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv NV21ToI420 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV12: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_data.y_buffer),
                        output_data.y_row_stride, buffer.dimension().width,
                        buffer.dimension().height);
      TFLITE_ASSIGN_OR_RETURN(
          const FrameBuffer::Dimension uv_plane_dimension,
          GetUvPlaneDimension(buffer.dimension(), buffer.format()));
      libyuv::SwapUVPlane(yuv_data.v_buffer, yuv_data.uv_row_stride,
                          const_cast<uint8*>(output_data.u_buffer),
                          output_data.uv_row_stride, uv_plane_dimension.width,
                          uv_plane_dimension.height);
      break;
    }
    case FrameBuffer::Format::kGRAY: {
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_buffer->plane(0).buffer),
                        output_buffer->plane(0).stride.row_stride_bytes,
                        output_buffer->dimension().width,
                        output_buffer->dimension().height);
      break;
    }
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.",
                          output_buffer->format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
  return absl::OkStatus();
}

// Converts YV12/YV21 `buffer` to the `output_buffer` of the target color space.
// Supported output format includes RGB24, NV12, and NV21.
absl::Status ConvertFromYv(const FrameBuffer& buffer,
                           FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData yuv_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  switch (output_buffer->format()) {
    case FrameBuffer::Format::kRGB: {
      // The RAW format of Libyuv represents the 8-bit interleaved RGB format in
      // the big endian style with R being the first byte in memory.
      int ret = libyuv::I420ToRAW(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride, yuv_data.v_buffer, yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv I420ToRAW operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kRGBA: {
      // The libyuv ABGR format is interleaved RGBA format in memory.
      int ret = libyuv::I420ToABGR(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride, yuv_data.v_buffer, yuv_data.uv_row_stride,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv I420ToABGR operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV12: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::I420ToNV12(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride, yuv_data.v_buffer, yuv_data.uv_row_stride,
          const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
          const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
          output_buffer->dimension().width, output_buffer->dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv I420ToNV12 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::I420ToNV21(
          yuv_data.y_buffer, yuv_data.y_row_stride, yuv_data.u_buffer,
          yuv_data.uv_row_stride, yuv_data.v_buffer, yuv_data.uv_row_stride,
          const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
          const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
          output_buffer->dimension().width, output_buffer->dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv I420ToNV21 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kGRAY: {
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_buffer->plane(0).buffer),
                        output_buffer->plane(0).stride.row_stride_bytes,
                        output_buffer->dimension().width,
                        output_buffer->dimension().height);
      break;
    }
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_yuv_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      TFLITE_ASSIGN_OR_RETURN(
          const FrameBuffer::Dimension uv_plane_dimension,
          GetUvPlaneDimension(buffer.dimension(), buffer.format()));
      libyuv::CopyPlane(yuv_data.y_buffer, yuv_data.y_row_stride,
                        const_cast<uint8*>(output_yuv_data.y_buffer),
                        output_yuv_data.y_row_stride, buffer.dimension().width,
                        buffer.dimension().height);
      libyuv::CopyPlane(yuv_data.u_buffer, yuv_data.uv_row_stride,
                        const_cast<uint8*>(output_yuv_data.u_buffer),
                        output_yuv_data.uv_row_stride, uv_plane_dimension.width,
                        uv_plane_dimension.height);
      libyuv::CopyPlane(yuv_data.v_buffer, yuv_data.uv_row_stride,
                        const_cast<uint8*>(output_yuv_data.v_buffer),
                        output_yuv_data.uv_row_stride, uv_plane_dimension.width,
                        uv_plane_dimension.height);
      break;
    }
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.",
                          output_buffer->format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
  return absl::OkStatus();
}

// Resizes YV12/YV21 `buffer` to the target `output_buffer`.
absl::Status ResizeYv(
    const FrameBuffer& buffer, FrameBuffer* output_buffer,
    libyuv::FilterMode interpolation = libyuv::FilterMode::kFilterBilinear) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  // TODO(b/151217096): Choose the optimal image resizing filter to optimize
  // the model inference performance.
  int ret = libyuv::I420Scale(
      input_data.y_buffer, input_data.y_row_stride, input_data.u_buffer,
      input_data.uv_row_stride, input_data.v_buffer, input_data.uv_row_stride,
      buffer.dimension().width, buffer.dimension().height,
      const_cast<uint8_t*>(output_data.y_buffer), output_data.y_row_stride,
      const_cast<uint8_t*>(output_data.u_buffer), output_data.uv_row_stride,
      const_cast<uint8_t*>(output_data.v_buffer), output_data.uv_row_stride,
      output_buffer->dimension().width, output_buffer->dimension().height,
      interpolation);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv I420Scale operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Resizes NV12/NV21 `buffer` to the target `output_buffer`.
absl::Status ResizeNv(
    const FrameBuffer& buffer, FrameBuffer* output_buffer,
    libyuv::FilterMode interpolation = libyuv::FilterMode::kFilterBilinear) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  const uint8* src_uv = input_data.u_buffer;
  const uint8* dst_uv = output_data.u_buffer;
  if (buffer.format() == FrameBuffer::Format::kNV21) {
    src_uv = input_data.v_buffer;
    dst_uv = output_data.v_buffer;
  }

  int ret = libyuv::NV12Scale(
      input_data.y_buffer, input_data.y_row_stride, src_uv,
      input_data.uv_row_stride, buffer.dimension().width,
      buffer.dimension().height, const_cast<uint8_t*>(output_data.y_buffer),
      output_data.y_row_stride, const_cast<uint8_t*>(dst_uv),
      output_data.uv_row_stride, output_buffer->dimension().width,
      output_buffer->dimension().height, interpolation);

  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv NV12Scale operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Converts `buffer` to libyuv ARGB format and stores the conversion result
// in `dest_argb`.
absl::Status ConvertRgbToArgb(const FrameBuffer& buffer, uint8* dest_argb,
                              int dest_stride_argb) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  if (buffer.format() != FrameBuffer::Format::kRGB) {
    return CreateStatusWithPayload(StatusCode::kInternal,
                                   "RGB input format is expected.",
                                   TfLiteSupportStatus::kImageProcessingError);
  }

  if (dest_argb == nullptr || dest_stride_argb <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        "Invalid destination arguments for ConvertRgbToArgb.",
        TfLiteSupportStatus::kImageProcessingError);
  }

  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  int ret = libyuv::RGB24ToARGB(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      dest_argb, dest_stride_argb, buffer.dimension().width,
      buffer.dimension().height);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv RGB24ToARGB operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Converts `src_argb` in libyuv ARGB format to FrameBuffer::kRGB format and
// stores the conversion result in `output_buffer`.
absl::Status ConvertArgbToRgb(uint8* src_argb, int src_stride_argb,
                              FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(*output_buffer));
  if (output_buffer->format() != FrameBuffer::Format::kRGB) {
    return absl::InternalError("RGB input format is expected.");
  }

  if (src_argb == nullptr || src_stride_argb <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kInternal, "Invalid source arguments for ConvertArgbToRgb.",
        TfLiteSupportStatus::kImageProcessingError);
  }

  if (output_buffer->plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        output_buffer->format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  int ret = libyuv::ARGBToRGB24(
      src_argb, src_stride_argb,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes,
      output_buffer->dimension().width, output_buffer->dimension().height);

  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBToRGB24 operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Converts `buffer` in FrameBuffer::kRGBA format to libyuv ARGB (BGRA in
// memory) format and stores the conversion result in `dest_argb`.
absl::Status ConvertRgbaToArgb(const FrameBuffer& buffer, uint8* dest_argb,
                               int dest_stride_argb) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  if (buffer.format() != FrameBuffer::Format::kRGBA) {
    return CreateStatusWithPayload(
        StatusCode::kInternal, "RGBA input format is expected.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  if (dest_argb == nullptr || dest_stride_argb <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        "Invalid source arguments for ConvertRgbaToArgb.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  int ret = libyuv::ABGRToARGB(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      dest_argb, dest_stride_argb, buffer.dimension().width,
      buffer.dimension().height);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kInternal, "Libyuv ABGRToARGB operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Converts kRGB `buffer` to the `output_buffer` of the target color space.
absl::Status ConvertFromRgb(const FrameBuffer& buffer,
                            FrameBuffer* output_buffer) {
  if (output_buffer->format() == FrameBuffer::Format::kGRAY) {
    int ret = libyuv::RAWToJ400(
        buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
        const_cast<uint8*>(output_buffer->plane(0).buffer),
        output_buffer->plane(0).stride.row_stride_bytes,
        buffer.dimension().width, buffer.dimension().height);
    if (ret != 0) {
      return CreateStatusWithPayload(
          StatusCode::kInternal, "Libyuv RAWToJ400 operation failed.",
          TfLiteSupportStatus::kImageProcessingBackendError);
    }
    return absl::OkStatus();
  } else if (output_buffer->format() == FrameBuffer::Format::kYV12 ||
             output_buffer->format() == FrameBuffer::Format::kYV21 ||
             output_buffer->format() == FrameBuffer::Format::kNV12 ||
             output_buffer->format() == FrameBuffer::Format::kNV21) {
    // libyuv does not support conversion directly from kRGB to kNV12 / kNV21.
    // For kNV12 / kNV21, the implementation converts the kRGB to I420,
    // then converts I420 to kNV12 / kNV21.
    // TODO(b/153000936): use libyuv::RawToNV12 / libyuv::RawToNV21 when they
    // are ready.
    FrameBuffer::YuvData yuv_data;
    std::unique_ptr<uint8[]> tmp_yuv_buffer;
    std::unique_ptr<FrameBuffer> yuv_frame_buffer;
    if (output_buffer->format() == FrameBuffer::Format::kNV12 ||
        output_buffer->format() == FrameBuffer::Format::kNV21) {
      tmp_yuv_buffer = absl::make_unique<uint8[]>(
          GetFrameBufferByteSize(buffer.dimension(), output_buffer->format()));
      TFLITE_ASSIGN_OR_RETURN(
          yuv_frame_buffer,
          CreateFromRawBuffer(tmp_yuv_buffer.get(), buffer.dimension(),
                              FrameBuffer::Format::kYV21,
                              output_buffer->orientation()));
      TFLITE_ASSIGN_OR_RETURN(
          yuv_data, FrameBuffer::GetYuvDataFromFrameBuffer(*yuv_frame_buffer));
    } else {
      TFLITE_ASSIGN_OR_RETURN(yuv_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
    }
    int ret = libyuv::RAWToI420(
        buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
        const_cast<uint8*>(yuv_data.y_buffer), yuv_data.y_row_stride,
        const_cast<uint8*>(yuv_data.u_buffer), yuv_data.uv_row_stride,
        const_cast<uint8*>(yuv_data.v_buffer), yuv_data.uv_row_stride,
        buffer.dimension().width, buffer.dimension().height);
    if (ret != 0) {
      return CreateStatusWithPayload(
          StatusCode::kInternal, "Libyuv RAWToI420 operation failed.",
          TfLiteSupportStatus::kImageProcessingBackendError);
    }
    if (output_buffer->format() == FrameBuffer::Format::kNV12 ||
        output_buffer->format() == FrameBuffer::Format::kNV21) {
      return ConvertFromYv(*yuv_frame_buffer, output_buffer);
    }
    return absl::OkStatus();
  } else if (output_buffer->format() == FrameBuffer::Format::kRGBA) {
    // RGB24 is BGR in memory and ARGB is BGRA in memory. The additional of the
    // alpha channel will not impact the RGB ordering.
    int ret = libyuv::RGB24ToARGB(
        buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
        const_cast<uint8*>(output_buffer->plane(0).buffer),
        output_buffer->plane(0).stride.row_stride_bytes,
        buffer.dimension().width, buffer.dimension().height);
    if (ret != 0) {
      return CreateStatusWithPayload(
          StatusCode::kInternal, "Libyuv RAWToARGB operation failed.",
          TfLiteSupportStatus::kImageProcessingBackendError);
    }
    return absl::OkStatus();
  }
  return CreateStatusWithPayload(
      StatusCode::kInternal,
      absl::StrFormat("Format %i is not supported.", output_buffer->format()),
      TfLiteSupportStatus::kImageProcessingError);
}

// Converts kRGBA `buffer` to the `output_buffer` of the target color space.
absl::Status ConvertFromRgba(const FrameBuffer& buffer,
                             FrameBuffer* output_buffer) {
  switch (output_buffer->format()) {
    case FrameBuffer::Format::kGRAY: {
      // libyuv does not support convert kRGBA (ABGR) foramat. In this method,
      // the implementation converts kRGBA format to ARGB and use ARGB buffer
      // for conversion.
      // TODO(b/141181395): Use libyuv::ABGRToJ400 when it is ready.

      // Convert kRGBA to ARGB
      int argb_buffer_size = GetFrameBufferByteSize(buffer.dimension(),
                                                    FrameBuffer::Format::kRGBA);
      auto argb_buffer = absl::make_unique<uint8[]>(argb_buffer_size);
      const int argb_row_bytes = buffer.dimension().width * kRgbaPixelBytes;
      TFLITE_RETURN_IF_ERROR(
          ConvertRgbaToArgb(buffer, argb_buffer.get(), argb_row_bytes));

      // Convert ARGB to kGRAY
      int ret = libyuv::ARGBToJ400(
          argb_buffer.get(), argb_row_bytes,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv ARGBToJ400 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV12: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::ABGRToNV12(
          buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
          const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
          const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv ABGRToNV12 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kNV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::ABGRToNV21(
          buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
          const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
          const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv ABGRToNV21 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21: {
      TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                       FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
      int ret = libyuv::ABGRToI420(
          buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
          const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
          const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
          const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv ABGRToI420 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    case FrameBuffer::Format::kRGB: {
      // ARGB is BGRA in memory and RGB24 is BGR in memory. The removal of the
      // alpha channel will not impact the RGB ordering.
      int ret = libyuv::ARGBToRGB24(
          buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
          const_cast<uint8*>(output_buffer->plane(0).buffer),
          output_buffer->plane(0).stride.row_stride_bytes,
          buffer.dimension().width, buffer.dimension().height);
      if (ret != 0) {
        return CreateStatusWithPayload(
            StatusCode::kUnknown, "Libyuv ABGRToRGB24 operation failed.",
            TfLiteSupportStatus::kImageProcessingBackendError);
      }
      break;
    }
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Convert Rgba to format %i is not supported.",
                          output_buffer->format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
  return absl::OkStatus();
}

// Returns libyuv rotation based on counter-clockwise angle_deg.
libyuv::RotationMode GetLibyuvRotationMode(int angle_deg) {
  switch (angle_deg) {
    case 90:
      return libyuv::kRotate270;
    case 270:
      return libyuv::kRotate90;
    case 180:
      return libyuv::kRotate180;
    default:
      return libyuv::kRotate0;
  }
}

absl::Status RotateRgba(const FrameBuffer& buffer, int angle_deg,
                        FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  // libyuv::ARGBRotate assumes RGBA buffer is in the interleaved format.
  int ret = libyuv::ARGBRotate(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes, buffer.dimension().width,
      buffer.dimension().height, GetLibyuvRotationMode(angle_deg % 360));
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBRotate operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

absl::Status RotateRgb(const FrameBuffer& buffer, int angle_deg,
                       FrameBuffer* output_buffer) {
  // libyuv does not support rotate kRGB (RGB24) foramat. In this method, the
  // implementation converts kRGB format to ARGB and use ARGB buffer for
  // rotation. The result is then convert back to RGB.

  // Convert RGB to ARGB
  int argb_buffer_size =
      GetFrameBufferByteSize(buffer.dimension(), FrameBuffer::Format::kRGBA);
  auto argb_buffer = absl::make_unique<uint8[]>(argb_buffer_size);
  const int argb_row_bytes = buffer.dimension().width * kRgbaPixelBytes;
  TFLITE_RETURN_IF_ERROR(ConvertRgbToArgb(buffer, argb_buffer.get(), argb_row_bytes));

  // Rotate ARGB
  auto argb_rotated_buffer = absl::make_unique<uint8[]>(argb_buffer_size);
  int rotated_row_bytes = output_buffer->dimension().width * kRgbaPixelBytes;
  // TODO(b/151954340): Optimize the current implementation by utilizing
  // ARGBMirror for 180 degree rotation.
  int ret = libyuv::ARGBRotate(
      argb_buffer.get(), argb_row_bytes, argb_rotated_buffer.get(),
      rotated_row_bytes, buffer.dimension().width, buffer.dimension().height,
      GetLibyuvRotationMode(angle_deg % 360));
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBRotate operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  // Convert ARGB to RGB
  return ConvertArgbToRgb(argb_rotated_buffer.get(), rotated_row_bytes,
                          output_buffer);
}

absl::Status RotateGray(const FrameBuffer& buffer, int angle_deg,
                        FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  int ret = libyuv::RotatePlane(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes, buffer.dimension().width,
      buffer.dimension().height, GetLibyuvRotationMode(angle_deg % 360));
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv RotatePlane operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Rotates YV12/YV21 frame buffer.
absl::Status RotateYv(const FrameBuffer& buffer, int angle_deg,
                      FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  int ret = libyuv::I420Rotate(
      input_data.y_buffer, input_data.y_row_stride, input_data.u_buffer,
      input_data.uv_row_stride, input_data.v_buffer, input_data.uv_row_stride,
      const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
      const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
      const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
      buffer.dimension().width, buffer.dimension().height,
      GetLibyuvRotationMode(angle_deg));
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv I420Rotate operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Rotates NV12/NV21 frame buffer.
// TODO(b/152097364): Refactor NV12/NV21 rotation after libyuv explicitly
// support that.
absl::Status RotateNv(const FrameBuffer& buffer, int angle_deg,
                      FrameBuffer* output_buffer) {
  if (buffer.format() != FrameBuffer::Format::kNV12 &&
      buffer.format() != FrameBuffer::Format::kNV21) {
    return CreateStatusWithPayload(StatusCode::kInternal,
                                   "kNV12 or kNV21 input formats are expected.",
                                   TfLiteSupportStatus::kImageProcessingError);
  }
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  const int rotated_buffer_size = GetFrameBufferByteSize(
      output_buffer->dimension(), FrameBuffer::Format::kYV21);
  auto rotated_yuv_raw_buffer = absl::make_unique<uint8[]>(rotated_buffer_size);
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<FrameBuffer> rotated_yuv_buffer,
                   CreateFromRawBuffer(
                       rotated_yuv_raw_buffer.get(), output_buffer->dimension(),
                       /*target_format=*/FrameBuffer::Format::kYV21,
                       output_buffer->orientation()));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData rotated_yuv_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*rotated_yuv_buffer));
  // Get the first chroma plane and use it as the u plane. This is a workaround
  // for optimizing NV21 rotation. For NV12, the implementation is logical
  // correct. For NV21, use v plane as u plane will make the UV planes swapped
  // in the intermediate rotated I420 frame. The output buffer is finally built
  // by merging the swapped UV planes which produces V first interleaved UV
  // buffer.
  const uint8* chroma_buffer = buffer.format() == FrameBuffer::Format::kNV12
                                   ? input_data.u_buffer
                                   : input_data.v_buffer;
  // Rotate the Y plane and store into the Y plane in `output_buffer`. Rotate
  // the interleaved UV plane and store into the interleaved UV plane in
  // `rotated_yuv_buffer`.
  int ret = libyuv::NV12ToI420Rotate(
      input_data.y_buffer, input_data.y_row_stride, chroma_buffer,
      input_data.uv_row_stride, const_cast<uint8*>(output_data.y_buffer),
      output_data.y_row_stride, const_cast<uint8*>(rotated_yuv_data.u_buffer),
      rotated_yuv_data.uv_row_stride,
      const_cast<uint8*>(rotated_yuv_data.v_buffer),
      rotated_yuv_data.uv_row_stride, buffer.dimension().width,
      buffer.dimension().height, GetLibyuvRotationMode(angle_deg % 360));
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv Nv12ToI420Rotate operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  // Merge rotated UV planes into the output buffer. For NV21, the UV buffer of
  // the intermediate I420 frame is swapped. MergeUVPlane builds the interleaved
  // VU buffer for NV21 by putting the U plane in the I420 frame which is
  // actually the V plane from the input buffer first.
  const uint8* output_chroma_buffer =
      buffer.format() == FrameBuffer::Format::kNV12 ? output_data.u_buffer
                                                    : output_data.v_buffer;
  // The width and height arguments of `libyuv::MergeUVPlane()` represent the
  // width and height of the UV planes.
  libyuv::MergeUVPlane(
      rotated_yuv_data.u_buffer, rotated_yuv_data.uv_row_stride,
      rotated_yuv_data.v_buffer, rotated_yuv_data.uv_row_stride,
      const_cast<uint8*>(output_chroma_buffer), output_data.uv_row_stride,
      (output_buffer->dimension().width + 1) / 2,
      (output_buffer->dimension().height + 1) / 2);
  return absl::OkStatus();
}

// This method only supports kGRAY, kRGB, and kRGBA format.
absl::Status FlipPlaneVertically(const FrameBuffer& buffer,
                                 FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  TFLITE_ASSIGN_OR_RETURN(int pixel_stride, GetPixelStrides(buffer.format()));

  // Flip vertically is achieved by passing in negative height.
  libyuv::CopyPlane(buffer.plane(0).buffer,
                    buffer.plane(0).stride.row_stride_bytes,
                    const_cast<uint8*>(output_buffer->plane(0).buffer),
                    output_buffer->plane(0).stride.row_stride_bytes,
                    output_buffer->dimension().width * pixel_stride,
                    -output_buffer->dimension().height);

  return absl::OkStatus();
}

// This method only supports kGRAY, kRGBA, and kRGB formats.
absl::Status CropPlane(const FrameBuffer& buffer, int x0, int y0, int x1,
                       int y1, FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  TFLITE_ASSIGN_OR_RETURN(int pixel_stride, GetPixelStrides(buffer.format()));
  FrameBuffer::Dimension crop_dimension = GetCropDimension(x0, x1, y0, y1);

  // Cropping is achieved by adjusting origin to (x0, y0).
  int adjusted_offset =
      buffer.plane(0).stride.row_stride_bytes * y0 + x0 * pixel_stride;

  libyuv::CopyPlane(buffer.plane(0).buffer + adjusted_offset,
                    buffer.plane(0).stride.row_stride_bytes,
                    const_cast<uint8*>(output_buffer->plane(0).buffer),
                    output_buffer->plane(0).stride.row_stride_bytes,
                    crop_dimension.width * pixel_stride, crop_dimension.height);

  return absl::OkStatus();
}

// Crops NV12/NV21 FrameBuffer to the subregion defined by the top left pixel
// position (x0, y0) and the bottom right pixel position (x1, y1).
absl::Status CropNv(const FrameBuffer& buffer, int x0, int y0, int x1, int y1,
                    FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  // Crop Y plane by copying the buffer with the origin offset to (x0, y0).
  int crop_offset_y = input_data.y_row_stride * y0 + x0;
  int crop_width = x1 - x0 + 1;
  int crop_height = y1 - y0 + 1;
  libyuv::CopyPlane(input_data.y_buffer + crop_offset_y,
                    input_data.y_row_stride,
                    const_cast<uint8*>(output_data.y_buffer),
                    output_data.y_row_stride, crop_width, crop_height);
  // Crop chroma plane by copying the buffer with the origin offset to
  // (x0 / 2, y0 / 2);
  // TODO(b/152629712): Investigate the impact of color shifting caused by the
  // bounding box with odd X or Y starting positions.
  int crop_offset_chroma = input_data.uv_row_stride * (y0 / 2) +
                           input_data.uv_pixel_stride * (x0 / 2);
  TFLITE_ASSIGN_OR_RETURN(const uint8* input_chroma_buffer, GetUvRawBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(const uint8* output_chroma_buffer,
                   GetUvRawBuffer(*output_buffer));
  libyuv::CopyPlane(
      input_chroma_buffer + crop_offset_chroma, input_data.uv_row_stride,
      const_cast<uint8*>(output_chroma_buffer), output_data.uv_row_stride,
      /*width=*/(crop_width + 1) / 2 * 2, /*height=*/(crop_height + 1) / 2);
  return absl::OkStatus();
}

// Crops YV12/YV21 FrameBuffer to the subregion defined by the top left pixel
// position (x0, y0) and the bottom right pixel position (x1, y1).
absl::Status CropYv(const FrameBuffer& buffer, int x0, int y0, int x1, int y1,
                    FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  // Crop Y plane by copying the buffer with the origin offset to (x0, y0).
  int crop_offset_y = input_data.y_row_stride * y0 + x0;
  FrameBuffer::Dimension crop_dimension = GetCropDimension(x0, x1, y0, y1);
  libyuv::CopyPlane(
      input_data.y_buffer + crop_offset_y, input_data.y_row_stride,
      const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
      crop_dimension.width, crop_dimension.height);
  // Crop U plane by copying the buffer with the origin offset to
  // (x0 / 2, y0 / 2).
  TFLITE_ASSIGN_OR_RETURN(const FrameBuffer::Dimension crop_uv_dimension,
                   GetUvPlaneDimension(crop_dimension, buffer.format()));
  // TODO(b/152629712): Investigate the impact of color shifting caused by the
  // bounding box with odd X or Y starting positions.
  int crop_offset_chroma = input_data.uv_row_stride * (y0 / 2) +
                           input_data.uv_pixel_stride * (x0 / 2);
  libyuv::CopyPlane(
      input_data.u_buffer + crop_offset_chroma, input_data.uv_row_stride,
      const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
      crop_uv_dimension.width, crop_uv_dimension.height);
  // Crop V plane by copying the buffer with the origin offset to
  // (x0 / 2, y0 / 2);
  libyuv::CopyPlane(
      input_data.v_buffer + crop_offset_chroma, input_data.uv_row_stride,
      const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
      /*width=*/(crop_dimension.width + 1) / 2,
      /*height=*/(crop_dimension.height + 1) / 2);
  return absl::OkStatus();
}

absl::Status CropResizeYuv(const FrameBuffer& buffer, int x0, int y0, int x1,
                           int y1, FrameBuffer* output_buffer) {
  FrameBuffer::Dimension crop_dimension = GetCropDimension(x0, x1, y0, y1);
  if (crop_dimension == output_buffer->dimension()) {
    switch (buffer.format()) {
      case FrameBuffer::Format::kNV12:
      case FrameBuffer::Format::kNV21:
        return CropNv(buffer, x0, y0, x1, y1, output_buffer);
      case FrameBuffer::Format::kYV12:
      case FrameBuffer::Format::kYV21:
        return CropYv(buffer, x0, y0, x1, y1, output_buffer);
      default:
        return CreateStatusWithPayload(
            StatusCode::kInternal,
            absl::StrFormat("Format %i is not supported.", buffer.format()),
            TfLiteSupportStatus::kImageProcessingError);
    }
  }
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  // Cropping YUV planes by offsetting the origins of each plane.
  // TODO(b/152629712): Investigate the impact of color shifting caused by the
  // bounding box with odd X or Y starting positions.
  const int plane_y_offset = input_data.y_row_stride * y0 + x0;
  const int plane_uv_offset = input_data.uv_row_stride * (y0 / 2) +
                              input_data.uv_pixel_stride * (x0 / 2);
  FrameBuffer::Plane cropped_plane_y = {
      /*buffer=*/input_data.y_buffer + plane_y_offset,
      /*stride=*/{input_data.y_row_stride, /*pixel_stride_bytes=*/1}};
  FrameBuffer::Plane cropped_plane_u = {
      /*buffer=*/input_data.u_buffer + plane_uv_offset,
      /*stride=*/{input_data.uv_row_stride, input_data.uv_pixel_stride}};
  FrameBuffer::Plane cropped_plane_v = {
      /*buffer=*/input_data.v_buffer + plane_uv_offset,
      /*stride=*/{input_data.uv_row_stride, input_data.uv_pixel_stride}};

  switch (buffer.format()) {
    case FrameBuffer::Format::kNV12: {
      std::unique_ptr<FrameBuffer> cropped_buffer = FrameBuffer::Create(
          {cropped_plane_y, cropped_plane_u, cropped_plane_v}, crop_dimension,
          buffer.format(), buffer.orientation());
      return ResizeNv(*cropped_buffer, output_buffer);
    }
    case FrameBuffer::Format::kNV21: {
      std::unique_ptr<FrameBuffer> cropped_buffer = FrameBuffer::Create(
          {cropped_plane_y, cropped_plane_v, cropped_plane_u}, crop_dimension,
          buffer.format(), buffer.orientation());
      return ResizeNv(*cropped_buffer, output_buffer);
    }
    case FrameBuffer::Format::kYV12: {
      std::unique_ptr<FrameBuffer> cropped_buffer = FrameBuffer::Create(
          {cropped_plane_y, cropped_plane_v, cropped_plane_u}, crop_dimension,
          buffer.format(), buffer.orientation());
      return ResizeYv(*cropped_buffer, output_buffer);
    }
    case FrameBuffer::Format::kYV21: {
      std::unique_ptr<FrameBuffer> cropped_buffer = FrameBuffer::Create(
          {cropped_plane_y, cropped_plane_u, cropped_plane_v}, crop_dimension,
          buffer.format(), buffer.orientation());
      return ResizeYv(*cropped_buffer, output_buffer);
    }
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
  return absl::OkStatus();
}

absl::Status FlipHorizontallyRgba(const FrameBuffer& buffer,
                                  FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  int ret = libyuv::ARGBMirror(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes,
      output_buffer->dimension().width, output_buffer->dimension().height);

  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBMirror operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  return absl::OkStatus();
}

// Flips `buffer` horizontally and store the result in `output_buffer`. This
// method assumes all buffers have pixel stride equals to 1.
absl::Status FlipHorizontallyPlane(const FrameBuffer& buffer,
                                   FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  libyuv::MirrorPlane(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes,
      output_buffer->dimension().width, output_buffer->dimension().height);

  return absl::OkStatus();
}

absl::Status ResizeRgb(
    const FrameBuffer& buffer, FrameBuffer* output_buffer,
    libyuv::FilterMode interpolation = libyuv::FilterMode::kFilterBilinear) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

  // libyuv doesn't support scale kRGB (RGB24) foramat. In this method,
  // the implementation converts kRGB format to ARGB and use ARGB buffer for
  // scaling. The result is then convert back to RGB.

  // Convert RGB to ARGB
  int argb_buffer_size =
      GetFrameBufferByteSize(buffer.dimension(), FrameBuffer::Format::kRGBA);
  auto argb_buffer = absl::make_unique<uint8[]>(argb_buffer_size);
  const int argb_row_bytes = buffer.dimension().width * kRgbaPixelBytes;
  TFLITE_RETURN_IF_ERROR(ConvertRgbToArgb(buffer, argb_buffer.get(), argb_row_bytes));

  // Resize ARGB
  int resized_argb_buffer_size = GetFrameBufferByteSize(
      output_buffer->dimension(), FrameBuffer::Format::kRGBA);
  auto resized_argb_buffer =
      absl::make_unique<uint8[]>(resized_argb_buffer_size);
  int resized_argb_row_bytes =
      output_buffer->dimension().width * kRgbaPixelBytes;
  int ret = libyuv::ARGBScale(
      argb_buffer.get(), argb_row_bytes, buffer.dimension().width,
      buffer.dimension().height, resized_argb_buffer.get(),
      resized_argb_row_bytes, output_buffer->dimension().width,
      output_buffer->dimension().height, interpolation);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBScale operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  // Convert ARGB to RGB
  return ConvertArgbToRgb(resized_argb_buffer.get(), resized_argb_row_bytes,
                          output_buffer);
}

// Horizontally flip `buffer` and store the result in `output_buffer`.
absl::Status FlipHorizontallyRgb(const FrameBuffer& buffer,
                                 FrameBuffer* output_buffer) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }

#if LIBYUV_VERSION >= 1747
  int ret = libyuv::RGB24Mirror(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes, buffer.dimension().width,
      buffer.dimension().height);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv RGB24Mirror operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  return absl::OkStatus();
#else
#error LibyuvFrameBufferUtils requires LIBYUV_VERSION 1747 or above
#endif  // LIBYUV_VERSION >= 1747
}

absl::Status ResizeRgba(
    const FrameBuffer& buffer, FrameBuffer* output_buffer,
    libyuv::FilterMode interpolation = libyuv::FilterMode::kFilterBilinear) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  int ret = libyuv::ARGBScale(
      buffer.plane(0).buffer, buffer.plane(0).stride.row_stride_bytes,
      buffer.dimension().width, buffer.dimension().height,
      const_cast<uint8*>(output_buffer->plane(0).buffer),
      output_buffer->plane(0).stride.row_stride_bytes,
      output_buffer->dimension().width, output_buffer->dimension().height,
      interpolation);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv ARGBScale operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Flips NV12/NV21 FrameBuffer horizontally.
absl::Status FlipHorizontallyNv(const FrameBuffer& buffer,
                                FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  TFLITE_ASSIGN_OR_RETURN(const uint8* input_chroma_buffer, GetUvRawBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(const uint8* output_chroma_buffer,
                   GetUvRawBuffer(*output_buffer));

  int ret = libyuv::NV12Mirror(
      input_data.y_buffer, input_data.y_row_stride, input_chroma_buffer,
      input_data.uv_row_stride, const_cast<uint8*>(output_data.y_buffer),
      output_data.y_row_stride, const_cast<uint8*>(output_chroma_buffer),
      output_data.uv_row_stride, buffer.dimension().width,
      buffer.dimension().height);

  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv NV12Mirror operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  return absl::OkStatus();
}

// Flips YV12/YV21 FrameBuffer horizontally.
absl::Status FlipHorizontallyYv(const FrameBuffer& buffer,
                                FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  int ret = libyuv::I420Mirror(
      input_data.y_buffer, input_data.y_row_stride, input_data.u_buffer,
      input_data.uv_row_stride, input_data.v_buffer, input_data.uv_row_stride,
      const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
      const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
      const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
      buffer.dimension().width, buffer.dimension().height);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv I420Mirror operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }

  return absl::OkStatus();
}

// Flips NV12/NV21 FrameBuffer vertically.
absl::Status FlipVerticallyNv(const FrameBuffer& buffer,
                              FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  // Flip Y plane vertically by passing a negative height.
  libyuv::CopyPlane(input_data.y_buffer, input_data.y_row_stride,
                    const_cast<uint8*>(output_data.y_buffer),
                    output_data.y_row_stride, buffer.dimension().width,
                    -output_buffer->dimension().height);
  // Flip UV plane vertically by passing a negative height.
  TFLITE_ASSIGN_OR_RETURN(const uint8* input_chroma_buffer, GetUvRawBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(const uint8* output_chroma_buffer,
                   GetUvRawBuffer(*output_buffer));
  TFLITE_ASSIGN_OR_RETURN(const FrameBuffer::Dimension uv_plane_dimension,
                   GetUvPlaneDimension(buffer.dimension(), buffer.format()));
  libyuv::CopyPlane(
      input_chroma_buffer, input_data.uv_row_stride,
      const_cast<uint8*>(output_chroma_buffer), output_data.uv_row_stride,
      /*width=*/uv_plane_dimension.width * 2, -uv_plane_dimension.height);
  return absl::OkStatus();
}

// Flips NV12/NV21 FrameBuffer vertically.
absl::Status FlipVerticallyYv(const FrameBuffer& buffer,
                              FrameBuffer* output_buffer) {
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData input_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(buffer));
  TFLITE_ASSIGN_OR_RETURN(FrameBuffer::YuvData output_data,
                   FrameBuffer::GetYuvDataFromFrameBuffer(*output_buffer));
  // Flip buffer vertically by passing a negative height.
  int ret = libyuv::I420Copy(
      input_data.y_buffer, input_data.y_row_stride, input_data.u_buffer,
      input_data.uv_row_stride, input_data.v_buffer, input_data.uv_row_stride,
      const_cast<uint8*>(output_data.y_buffer), output_data.y_row_stride,
      const_cast<uint8*>(output_data.u_buffer), output_data.uv_row_stride,
      const_cast<uint8*>(output_data.v_buffer), output_data.uv_row_stride,
      buffer.dimension().width, -buffer.dimension().height);
  if (ret != 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown, "Libyuv I420Copy operation failed.",
        TfLiteSupportStatus::kImageProcessingBackendError);
  }
  return absl::OkStatus();
}

// Resize `buffer` to metadata defined in `output_buffer`. This
// method assumes buffer has pixel stride equals to 1 (grayscale equivalent).
absl::Status ResizeGray(
    const FrameBuffer& buffer, FrameBuffer* output_buffer,
    libyuv::FilterMode interpolation = libyuv::FilterMode::kFilterBilinear) {
  if (buffer.plane_count() > 1) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Only single plane is supported for format %i.",
                        buffer.format()),
        TfLiteSupportStatus::kImageProcessingError);
  }
  libyuv::ScalePlane(buffer.plane(0).buffer,
                     buffer.plane(0).stride.row_stride_bytes,
                     buffer.dimension().width, buffer.dimension().height,
                     const_cast<uint8*>(output_buffer->plane(0).buffer),
                     output_buffer->plane(0).stride.row_stride_bytes,
                     output_buffer->dimension().width,
                     output_buffer->dimension().height, interpolation);
  return absl::OkStatus();
}

// This method only supports kGRAY, kRGBA, and kRGB formats.
absl::Status CropResize(const FrameBuffer& buffer, int x0, int y0, int x1,
                        int y1, FrameBuffer* output_buffer) {
  FrameBuffer::Dimension crop_dimension = GetCropDimension(x0, x1, y0, y1);
  if (crop_dimension == output_buffer->dimension()) {
    return CropPlane(buffer, x0, y0, x1, y1, output_buffer);
  }

  TFLITE_ASSIGN_OR_RETURN(int pixel_stride, GetPixelStrides(buffer.format()));
  // Cropping is achieved by adjusting origin to (x0, y0).
  int adjusted_offset =
      buffer.plane(0).stride.row_stride_bytes * y0 + x0 * pixel_stride;
  FrameBuffer::Plane plane = {
      /*buffer=*/buffer.plane(0).buffer + adjusted_offset,
      /*stride=*/{buffer.plane(0).stride.row_stride_bytes, pixel_stride}};
  auto adjusted_buffer =
      FrameBuffer::Create({plane}, crop_dimension, buffer.format(),
                          buffer.orientation(), buffer.timestamp());

  switch (buffer.format()) {
    case FrameBuffer::Format::kRGB:
      return ResizeRgb(*adjusted_buffer, output_buffer);
    case FrameBuffer::Format::kRGBA:
      return ResizeRgba(*adjusted_buffer, output_buffer);
    case FrameBuffer::Format::kGRAY:
      return ResizeGray(*adjusted_buffer, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

}  // namespace

absl::Status LibyuvFrameBufferUtils::Crop(const FrameBuffer& buffer, int x0,
                                          int y0, int x1, int y1,
                                          FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(*output_buffer));
  TFLITE_RETURN_IF_ERROR(
      ValidateCropBufferInputs(buffer, *output_buffer, x0, y0, x1, y1));
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormats(buffer, *output_buffer));

  switch (buffer.format()) {
    case FrameBuffer::Format::kRGBA:
    case FrameBuffer::Format::kRGB:
    case FrameBuffer::Format::kGRAY:
      return CropResize(buffer, x0, y0, x1, y1, output_buffer);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return CropResizeYuv(buffer, x0, y0, x1, y1, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::Resize(const FrameBuffer& buffer,
                                            FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateResizeBufferInputs(buffer, *output_buffer));
  switch (buffer.format()) {
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return ResizeYv(buffer, output_buffer);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
      return ResizeNv(buffer, output_buffer);
    case FrameBuffer::Format::kRGB:
      return ResizeRgb(buffer, output_buffer);
    case FrameBuffer::Format::kRGBA:
      return ResizeRgba(buffer, output_buffer);
    case FrameBuffer::Format::kGRAY:
      return ResizeGray(buffer, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::ResizeNearestNeighbor(
    const FrameBuffer& buffer, FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateResizeBufferInputs(buffer, *output_buffer));
  switch (buffer.format()) {
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return ResizeYv(buffer, output_buffer, libyuv::FilterMode::kFilterNone);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
      return ResizeNv(buffer, output_buffer, libyuv::FilterMode::kFilterNone);
    case FrameBuffer::Format::kRGB:
      return ResizeRgb(buffer, output_buffer, libyuv::FilterMode::kFilterNone);
    case FrameBuffer::Format::kRGBA:
      return ResizeRgba(buffer, output_buffer, libyuv::FilterMode::kFilterNone);
    case FrameBuffer::Format::kGRAY:
      return ResizeGray(buffer, output_buffer, libyuv::FilterMode::kFilterNone);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::Rotate(const FrameBuffer& buffer,
                                            int angle_deg,
                                            FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(
      ValidateRotateBufferInputs(buffer, *output_buffer, angle_deg));
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormats(buffer, *output_buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(*output_buffer));

  switch (buffer.format()) {
    case FrameBuffer::Format::kGRAY:
      return RotateGray(buffer, angle_deg, output_buffer);
    case FrameBuffer::Format::kRGBA:
      return RotateRgba(buffer, angle_deg, output_buffer);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
      return RotateNv(buffer, angle_deg, output_buffer);
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return RotateYv(buffer, angle_deg, output_buffer);
    case FrameBuffer::Format::kRGB:
      return RotateRgb(buffer, angle_deg, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::FlipHorizontally(
    const FrameBuffer& buffer, FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(*output_buffer));
  TFLITE_RETURN_IF_ERROR(ValidateFlipBufferInputs(buffer, *output_buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormats(buffer, *output_buffer));

  switch (buffer.format()) {
    case FrameBuffer::Format::kRGBA:
      return FlipHorizontallyRgba(buffer, output_buffer);
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return FlipHorizontallyYv(buffer, output_buffer);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
      return FlipHorizontallyNv(buffer, output_buffer);
    case FrameBuffer::Format::kRGB:
      return FlipHorizontallyRgb(buffer, output_buffer);
    case FrameBuffer::Format::kGRAY:
      return FlipHorizontallyPlane(buffer, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::FlipVertically(
    const FrameBuffer& buffer, FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferPlaneMetadata(*output_buffer));
  TFLITE_RETURN_IF_ERROR(ValidateFlipBufferInputs(buffer, *output_buffer));
  TFLITE_RETURN_IF_ERROR(ValidateBufferFormats(buffer, *output_buffer));

  switch (buffer.format()) {
    case FrameBuffer::Format::kRGBA:
    case FrameBuffer::Format::kRGB:
    case FrameBuffer::Format::kGRAY:
      return FlipPlaneVertically(buffer, output_buffer);
    case FrameBuffer::Format::kNV12:
    case FrameBuffer::Format::kNV21:
      return FlipVerticallyNv(buffer, output_buffer);
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return FlipVerticallyYv(buffer, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

absl::Status LibyuvFrameBufferUtils::Convert(const FrameBuffer& buffer,
                                             FrameBuffer* output_buffer) {
  TFLITE_RETURN_IF_ERROR(
      ValidateConvertFormats(buffer.format(), output_buffer->format()));
  switch (buffer.format()) {
    case FrameBuffer::Format::kNV12:
      return ConvertFromNv12(buffer, output_buffer);
    case FrameBuffer::Format::kNV21:
      return ConvertFromNv21(buffer, output_buffer);
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21:
      return ConvertFromYv(buffer, output_buffer);
    case FrameBuffer::Format::kRGB:
      return ConvertFromRgb(buffer, output_buffer);
    case FrameBuffer::Format::kRGBA:
      return ConvertFromRgba(buffer, output_buffer);
    default:
      return CreateStatusWithPayload(
          StatusCode::kInternal,
          absl::StrFormat("Format %i is not supported.", buffer.format()),
          TfLiteSupportStatus::kImageProcessingError);
  }
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
