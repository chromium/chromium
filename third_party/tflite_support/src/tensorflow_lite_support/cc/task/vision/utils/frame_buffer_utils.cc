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

#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow/lite/kernels/internal/compatibility.h"
#include "tensorflow/lite/kernels/op_macros.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/libyuv_frame_buffer_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {

// Exif grouping to help determine rotation and flipping neededs between
// different orientations.
constexpr int kExifGroup[] = {1, 6, 3, 8, 2, 5, 4, 7};
// Exif group size.
constexpr int kExifGroupSize = 4;

// Returns orientation position in Exif group.
static int GetOrientationIndex(FrameBuffer::Orientation orientation) {
  const int* index = std::find(kExifGroup, kExifGroup + kExifGroupSize * 2,
                               static_cast<int>(orientation));
  if (index < kExifGroup + kExifGroupSize * 2) {
    return std::distance(kExifGroup, index);
  }
  return -1;
}

// Returns the coordinates of `box` respect to its containing image (dimension
// defined by `width` and `height`) orientation change. The `angle` is defined
// in counterclockwise degree in one of the values [0, 90, 180, 270].
//
// The below diagrams illustrate calling this method with 90 CCW degree.
//
// The [1]-[4] denotes image corners and 1 - 4 denotes the box corners. The *
// denotes the current origin.
//
//             width
//   [1]*----------------[2]
//    |                   |
//    |                   |
//    |        1*-----2   | height
//    |        | box  |   |
//    |        3------4   |
//   [3]-----------------[4]
//
// When rotate the above image by 90 CCW degree, the origin also changes
// respects to its containing coordinate space.
//
//         height
//   [2]*----------[4]
//    |             |
//    |     2*---4  |
//    |     |box |  |
//    |     |    |  | width
//    |     1----3  |
//    |             |
//    |             |
//    |             |
//   [1]-----------[3]
//
// The origin is always defined by the top left corner. After rotation, the
// box origin changed from 1 to 2.
// The new box origin is (x:box.origin_y, y:width - (box.origin_x + box.width).
// The new box dimension is (w: box.height, h: box.width).
//
static BoundingBox RotateBoundingBox(const BoundingBox& box, int angle,
                                     FrameBuffer::Dimension frame_dimension) {
  int rx = box.origin_x(), ry = box.origin_y(), rw = box.width(),
      rh = box.height();
  const int box_right_bound =
      frame_dimension.width - (box.origin_x() + box.width());
  const int box_bottom_bound =
      frame_dimension.height - (box.origin_y() + box.height());
  switch (angle) {
    case 90:
      rx = box.origin_y();
      ry = box_right_bound;
      using std::swap;
      swap(rw, rh);
      break;
    case 180:
      rx = box_right_bound;
      ry = box_bottom_bound;
      break;
    case 270:
      rx = box_bottom_bound;
      ry = box.origin_x();
      using std::swap;
      swap(rw, rh);
      break;
  }
  BoundingBox result;
  result.set_origin_x(rx);
  result.set_origin_y(ry);
  result.set_width(rw);
  result.set_height(rh);
  return result;
}

// Returns the input coordinates with respect to its containing image (dimension
// defined by `width` and `height`) orientation change. The `angle` is defined
// in counterclockwise degree in one of the values [0, 90, 180, 270].
//
// See `RotateBoundingBox` above for more details.
static void RotateCoordinates(int from_x, int from_y, int angle,
                              const FrameBuffer::Dimension& frame_dimension,
                              int* to_x, int* to_y) {
  switch (angle) {
    case 0:
      *to_x = from_x;
      *to_y = from_y;
      break;
    case 90:
      *to_x = from_y;
      *to_y = frame_dimension.width - from_x - 1;
      break;
    case 180:
      *to_x = frame_dimension.width - from_x - 1;
      *to_y = frame_dimension.height - from_y - 1;
      break;
    case 270:
      *to_x = frame_dimension.height - from_y - 1;
      *to_y = from_x;
      break;
  }
}

}  // namespace

int GetBufferByteSize(FrameBuffer::Dimension dimension,
                      FrameBuffer::Format format) {
  return GetFrameBufferByteSize(dimension, format);
}

FrameBufferUtils::FrameBufferUtils(ProcessEngine engine) {
  switch (engine) {
    case ProcessEngine::kLibyuv:
      utils_ = absl::make_unique<LibyuvFrameBufferUtils>();
      break;
    default:
      TF_LITE_FATAL(
          absl::StrFormat("Unexpected ProcessEngine: %d.", engine).c_str());
  }
}

BoundingBox OrientBoundingBox(const BoundingBox& from_box,
                              FrameBuffer::Orientation from_orientation,
                              FrameBuffer::Orientation to_orientation,
                              FrameBuffer::Dimension from_dimension) {
  BoundingBox to_box = from_box;
  OrientParams params = GetOrientParams(from_orientation, to_orientation);
  // First, rotate if needed.
  if (params.rotation_angle_deg > 0) {
    to_box =
        RotateBoundingBox(to_box, params.rotation_angle_deg, from_dimension);
  }
  // Then perform horizontal or vertical flip if needed.
  FrameBuffer::Dimension to_dimension = from_dimension;
  if (params.rotation_angle_deg == 90 || params.rotation_angle_deg == 270) {
    to_dimension.Swap();
  }
  if (params.flip == OrientParams::FlipType::kVertical) {
    to_box.set_origin_y(to_dimension.height -
                        (to_box.origin_y() + to_box.height()));
  }
  if (params.flip == OrientParams::FlipType::kHorizontal) {
    to_box.set_origin_x(to_dimension.width -
                        (to_box.origin_x() + to_box.width()));
  }
  return to_box;
}

BoundingBox OrientAndDenormalizeBoundingBox(
    float from_left, float from_top, float from_right, float from_bottom,
    FrameBuffer::Orientation from_orientation,
    FrameBuffer::Orientation to_orientation,
    FrameBuffer::Dimension from_dimension) {
  BoundingBox from_box;
  from_box.set_origin_x(from_left * from_dimension.width);
  from_box.set_origin_y(from_top * from_dimension.height);
  from_box.set_width(round(abs(from_right - from_left) * from_dimension.width));
  from_box.set_height(
      round(abs(from_bottom - from_top) * from_dimension.height));
  BoundingBox to_box = OrientBoundingBox(from_box, from_orientation,
                                         to_orientation, from_dimension);
  return to_box;
}

void OrientCoordinates(int from_x, int from_y,
                       FrameBuffer::Orientation from_orientation,
                       FrameBuffer::Orientation to_orientation,
                       FrameBuffer::Dimension from_dimension, int* to_x,
                       int* to_y) {
  *to_x = from_x;
  *to_y = from_y;
  OrientParams params = GetOrientParams(from_orientation, to_orientation);
  // First, rotate if needed.
  if (params.rotation_angle_deg > 0) {
    RotateCoordinates(from_x, from_y, params.rotation_angle_deg, from_dimension,
                      to_x, to_y);
  }
  // Then perform horizontal or vertical flip if needed.
  FrameBuffer::Dimension to_dimension = from_dimension;
  if (params.rotation_angle_deg == 90 || params.rotation_angle_deg == 270) {
    to_dimension.Swap();
  }
  if (params.flip == OrientParams::FlipType::kVertical) {
    *to_y = to_dimension.height - *to_y - 1;
  }
  if (params.flip == OrientParams::FlipType::kHorizontal) {
    *to_x = to_dimension.width - *to_x - 1;
  }
}

// The algorithm is based on grouping orientations into two groups with specific
// order. The two groups of orientation are {1, 6, 3, 8} and {2, 5, 4, 7}. See
// image (https://www.impulseadventure.com/photo/images/orient_flag.gif) for
// the visual grouping illustration.
//
// Each group contains elements can be transformed into one another by rotation.
// The elements order within a group is important such that the distance between
// the elements indicates the multiples of 90 degree needed to orient from one
// element to another. For example, to orient element 1 to element 6, a 90
// degree CCW rotation is needed.
//
// The corresponding order between the two groups is important such that the
// even index defined the need for horizontal flipping and the odd index defined
// the need for vertical flipping. For example, to orient element 1 to element 2
// (even index) a horizontal flipping is needed.
//
// The implementation determines the group and element index of from and to
// orientations. Based on the group and element index information, the above
// characteristic is used to calculate the rotation angle and the need for
// horizontal or vertical flipping.
OrientParams GetOrientParams(FrameBuffer::Orientation from_orientation,
                             FrameBuffer::Orientation to_orientation) {
  int from_index = GetOrientationIndex(from_orientation);
  int to_index = GetOrientationIndex(to_orientation);
  int angle = 0;
  absl::optional<OrientParams::FlipType> flip;

  TFLITE_DCHECK(from_index > -1 && to_index > -1);

  if ((from_index < kExifGroupSize && to_index < kExifGroupSize) ||
      (from_index >= kExifGroupSize && to_index >= kExifGroupSize)) {
    // Only needs rotation.

    // The orientations' position differences translates to how many
    // multiple of 90 degrees it needs for conversion. The position difference
    // calculation within a group is circular.
    angle = (kExifGroupSize - (from_index - to_index)) % kExifGroupSize * 90;
  } else {
    // Needs rotation and flipping.
    int from_index_mod = from_index % kExifGroupSize;
    int to_index_mod = to_index % kExifGroupSize;
    angle = (kExifGroupSize - (from_index_mod - to_index_mod)) %
            kExifGroupSize * 90;
    if (to_index_mod % 2 == 1) {
      flip = OrientParams::FlipType::kVertical;
    } else {
      flip = OrientParams::FlipType::kHorizontal;
    }
  }
  return {angle, flip};
}

bool RequireDimensionSwap(FrameBuffer::Orientation from_orientation,
                          FrameBuffer::Orientation to_orientation) {
  OrientParams params = GetOrientParams(from_orientation, to_orientation);
  return params.rotation_angle_deg == 90 || params.rotation_angle_deg == 270;
}

absl::Status FrameBufferUtils::Crop(const FrameBuffer& buffer, int x0, int y0,
                                    int x1, int y1,
                                    FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->Crop(buffer, x0, y0, x1, y1, output_buffer);
}

FrameBuffer::Dimension FrameBufferUtils::GetSize(
    const FrameBuffer& buffer, const FrameBufferOperation& operation) {
  FrameBuffer::Dimension dimension = buffer.dimension();
  if (absl::holds_alternative<OrientOperation>(operation)) {
    OrientParams params =
        GetOrientParams(buffer.orientation(),
                        absl::get<OrientOperation>(operation).to_orientation);
    if (params.rotation_angle_deg == 90 || params.rotation_angle_deg == 270) {
      dimension.Swap();
    }
  } else if (absl::holds_alternative<CropResizeOperation>(operation)) {
    const auto& crop_resize = absl::get<CropResizeOperation>(operation);
    dimension = crop_resize.resize_dimension;
  } else if (absl::holds_alternative<UniformCropResizeOperation>(operation)) {
    const auto& uniform_crop_resize =
        absl::get<UniformCropResizeOperation>(operation);
    dimension = uniform_crop_resize.output_dimension;
  }
  return dimension;
}

std::vector<FrameBuffer::Plane> FrameBufferUtils::GetPlanes(
    const uint8* buffer, FrameBuffer::Dimension dimension,
    FrameBuffer::Format format) {
  std::vector<FrameBuffer::Plane> planes;
  switch (format) {
    case FrameBuffer::Format::kGRAY:
      planes.push_back({/*buffer=*/buffer,
                        /*stride=*/{/*row_stride_bytes=*/dimension.width * 1,
                                    /*pixel_stride_bytes=*/1}});
      break;
    case FrameBuffer::Format::kRGB:
      planes.push_back({/*buffer=*/buffer,
                        /*stride=*/{/*row_stride_bytes=*/dimension.width * 3,
                                    /*pixel_stride_bytes=*/3}});
      break;
    case FrameBuffer::Format::kRGBA:
      planes.push_back({/*buffer=*/buffer,
                        /*stride=*/{/*row_stride_bytes=*/dimension.width * 4,
                                    /*pixel_stride_bytes=*/4}});
      break;
    case FrameBuffer::Format::kNV21:
    case FrameBuffer::Format::kNV12: {
      planes.push_back(
          {buffer, /*stride=*/{/*row_stride_bytes=*/dimension.width,
                               /*pixel_stride_bytes=*/1}});
      planes.push_back(
          {buffer + (dimension.width * dimension.height),
           /*stride=*/{/*row_stride_bytes=*/(dimension.width + 1) / 2 * 2,
                       /*pixel_stride_bytes=*/2}});
    } break;
    case FrameBuffer::Format::kYV12:
    case FrameBuffer::Format::kYV21: {
      const int y_buffer_size = dimension.width * dimension.height;
      const int uv_row_stride = (dimension.width + 1) / 2;
      const int uv_buffer_size = uv_row_stride * (dimension.height + 1) / 2;
      planes.push_back(
          {buffer, /*stride=*/{/*row_stride_bytes=*/dimension.width,
                               /*pixel_stride_bytes=*/1}});
      planes.push_back(
          {buffer + y_buffer_size, /*stride=*/{
               /*row_stride_bytes=*/uv_row_stride, /*pixel_stride_bytes=*/1}});
      planes.push_back(
          {buffer + y_buffer_size + uv_buffer_size, /*stride=*/{
               /*row_stride_bytes=*/uv_row_stride, /*pixel_stride_bytes=*/1}});
    } break;
    default:
      break;
  }
  return planes;
}

FrameBuffer::Orientation FrameBufferUtils::GetOrientation(
    const FrameBuffer& buffer, const FrameBufferOperation& operation) {
  if (absl::holds_alternative<OrientOperation>(operation)) {
    return absl::get<OrientOperation>(operation).to_orientation;
  }
  return buffer.orientation();
}

FrameBuffer::Format FrameBufferUtils::GetFormat(
    const FrameBuffer& buffer, const FrameBufferOperation& operation) {
  if (absl::holds_alternative<ConvertOperation>(operation)) {
    return absl::get<ConvertOperation>(operation).to_format;
  }
  return buffer.format();
}

absl::Status FrameBufferUtils::Execute(const FrameBuffer& buffer,
                                       const FrameBufferOperation& operation,
                                       FrameBuffer* output_buffer) {
  if (absl::holds_alternative<CropResizeOperation>(operation)) {
    const auto& params = absl::get<CropResizeOperation>(operation);
    TFLITE_RETURN_IF_ERROR(
        Crop(buffer, params.crop_origin_x, params.crop_origin_y,
             (params.crop_dimension.width + params.crop_origin_x - 1),
             (params.crop_dimension.height + params.crop_origin_y - 1),
             output_buffer));
  } else if (absl::holds_alternative<UniformCropResizeOperation>(operation)) {
    const auto& params = absl::get<UniformCropResizeOperation>(operation);
    TFLITE_RETURN_IF_ERROR(
        Crop(buffer, params.crop_origin_x, params.crop_origin_y,
             (params.crop_dimension.width + params.crop_origin_x - 1),
             (params.crop_dimension.height + params.crop_origin_y - 1),
             output_buffer));
  } else if (absl::holds_alternative<ConvertOperation>(operation)) {
    TFLITE_RETURN_IF_ERROR(Convert(buffer, output_buffer));
  } else if (absl::holds_alternative<OrientOperation>(operation)) {
    TFLITE_RETURN_IF_ERROR(Orient(buffer, output_buffer));
  } else {
    return absl::UnimplementedError(absl::StrFormat(
        "FrameBufferOperation %i is not supported.", operation.index()));
  }
  return absl::OkStatus();
}

absl::Status FrameBufferUtils::Resize(const FrameBuffer& buffer,
                                      FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->Resize(buffer, output_buffer);
}

absl::Status FrameBufferUtils::ResizeNearestNeighbor(
    const FrameBuffer& buffer, FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->ResizeNearestNeighbor(buffer, output_buffer);
}

absl::Status FrameBufferUtils::Rotate(const FrameBuffer& buffer,
                                      RotationDegree rotation,
                                      FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->Rotate(buffer, 90 * static_cast<int>(rotation), output_buffer);
}

absl::Status FrameBufferUtils::FlipHorizontally(const FrameBuffer& buffer,
                                                FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->FlipHorizontally(buffer, output_buffer);
}

absl::Status FrameBufferUtils::FlipVertically(const FrameBuffer& buffer,
                                              FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->FlipVertically(buffer, output_buffer);
}

absl::Status FrameBufferUtils::Convert(const FrameBuffer& buffer,
                                       FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);
  return utils_->Convert(buffer, output_buffer);
}

absl::Status FrameBufferUtils::Orient(const FrameBuffer& buffer,
                                      FrameBuffer* output_buffer) {
  TFLITE_DCHECK(utils_ != nullptr);

  OrientParams params =
      GetOrientParams(buffer.orientation(), output_buffer->orientation());
  if (params.rotation_angle_deg == 0 && !params.flip.has_value()) {
    // If no rotation or flip is needed, we will copy the buffer to
    // output_buffer.
    return utils_->Resize(buffer, output_buffer);
  }

  if (params.rotation_angle_deg == 0) {
    // Only perform flip operation.
    switch (*params.flip) {
      case OrientParams::FlipType::kHorizontal:
        return utils_->FlipHorizontally(buffer, output_buffer);
      case OrientParams::FlipType::kVertical:
        return utils_->FlipVertically(buffer, output_buffer);
    }
  }

  if (!params.flip.has_value()) {
    // Only perform rotation operation.
    return utils_->Rotate(buffer, params.rotation_angle_deg, output_buffer);
  }

  // Perform rotation and flip operations.
  // Create a temporary buffer to hold the rotation result.
  auto tmp_buffer = absl::make_unique<uint8[]>(
      GetBufferByteSize(output_buffer->dimension(), output_buffer->format()));
  auto tmp_frame_buffer = FrameBuffer::Create(
      GetPlanes(tmp_buffer.get(), output_buffer->dimension(),
                output_buffer->format()),
      output_buffer->dimension(), buffer.format(), buffer.orientation());

  TFLITE_RETURN_IF_ERROR(utils_->Rotate(buffer, params.rotation_angle_deg,
                                 tmp_frame_buffer.get()));
  if (params.flip == OrientParams::FlipType::kHorizontal) {
    return utils_->FlipHorizontally(*tmp_frame_buffer, output_buffer);
  } else {
    return utils_->FlipVertically(*tmp_frame_buffer, output_buffer);
  }
}

absl::Status FrameBufferUtils::Execute(
    const FrameBuffer& buffer,
    const std::vector<FrameBufferOperation>& operations,
    FrameBuffer* output_buffer) {
  // Reference variables to swapping input and output buffers for each command.
  FrameBuffer input_frame_buffer = buffer;
  FrameBuffer temp_frame_buffer = buffer;

  // Temporary buffers and its size to hold intermediate results.
  int buffer1_size = 0;
  int buffer2_size = 0;
  std::unique_ptr<uint8[]> buffer1;
  std::unique_ptr<uint8[]> buffer2;

  for (size_t i = 0; i < operations.size(); i++) {
    const FrameBufferOperation& operation = operations[i];

    // The first command's input is always passed in `buffer`. Before
    // process each command, the input_frame_buffer is pointed at the previous
    // command's output buffer.
    if (i == 0) {
      input_frame_buffer = buffer;
    } else {
      input_frame_buffer = temp_frame_buffer;
    }

    // Calculates the resulting metadata from the command and the input.
    FrameBuffer::Dimension new_size = GetSize(input_frame_buffer, operation);
    FrameBuffer::Orientation new_orientation =
        GetOrientation(input_frame_buffer, operation);
    FrameBuffer::Format new_format = GetFormat(input_frame_buffer, operation);
    int byte_size = GetBufferByteSize(new_size, new_format);

    // The last command's output buffer is always passed in `output_buffer`.
    // For other commands, we create temporary FrameBuffer for processing.
    if ((i + 1) == operations.size()) {
      temp_frame_buffer = *output_buffer;
      // Validate the `output_buffer` metadata mathes with command line chain
      // resulting metadata.
      if (temp_frame_buffer.format() != new_format ||
          temp_frame_buffer.orientation() != new_orientation ||
          temp_frame_buffer.dimension() != new_size) {
        return absl::InvalidArgumentError(
            "The output metadata does not match pipeline result metadata.");
      }
    } else {
      // Create a temporary buffer to hold intermediate results. For simplicity,
      // we only create one continuous memory with no padding for intermediate
      // results.
      //
      // We hold maximum 2 temporary buffers in memory at any given time.
      //
      // The pipeline is a linear chain. The output buffer from previous command
      // becomes the input buffer for the next command. We simply use odd / even
      // index to swap between buffers.
      std::vector<FrameBuffer::Plane> planes;
      if (i % 2 == 0) {
        if (buffer1_size < byte_size) {
          buffer1_size = byte_size;
          buffer1 = absl::make_unique<uint8[]>(byte_size);
        }
        planes = GetPlanes(buffer1.get(), new_size, new_format);
      } else {
        if (buffer2_size < byte_size) {
          buffer2_size = byte_size;
          buffer2 = absl::make_unique<uint8[]>(byte_size);
        }
        planes = GetPlanes(buffer2.get(), new_size, new_format);
      }
      if (planes.empty()) {
        return absl::InternalError("Failed to construct temporary buffer.");
      }
      temp_frame_buffer = FrameBuffer(planes, new_size, new_format,
                                      new_orientation, buffer.timestamp());
    }
    TFLITE_RETURN_IF_ERROR(Execute(input_frame_buffer, operation, &temp_frame_buffer));
  }
  return absl::OkStatus();
}

absl::Status FrameBufferUtils::Preprocess(
    const FrameBuffer& buffer, absl::optional<BoundingBox> bounding_box,
    FrameBuffer* output_buffer, bool uniform_resizing) {
  std::vector<FrameBufferOperation> frame_buffer_operations;
  // Handle cropping and resizing.
  bool needs_dimension_swap =
      RequireDimensionSwap(buffer.orientation(), output_buffer->orientation());
  // For intermediate steps, we need to use dimensions based on the input
  // orientation.
  FrameBuffer::Dimension pre_orient_dimension = output_buffer->dimension();
  if (needs_dimension_swap) {
    pre_orient_dimension.Swap();
  }

  if (uniform_resizing && bounding_box.has_value()) {
    // Crop and uniform resize.
    frame_buffer_operations.push_back(UniformCropResizeOperation(
        bounding_box.value().origin_x(), bounding_box.value().origin_y(),
        FrameBuffer::Dimension{bounding_box.value().width(),
                               bounding_box.value().height()},
        pre_orient_dimension));
  } else if (uniform_resizing) {
    // Uniform resize only.
    frame_buffer_operations.push_back(UniformCropResizeOperation(
        0, 0, buffer.dimension(), pre_orient_dimension));
  } else if (bounding_box.has_value()) {
    // Crop and non-uniform resize.
    frame_buffer_operations.push_back(CropResizeOperation(
        bounding_box.value().origin_x(), bounding_box.value().origin_y(),
        FrameBuffer::Dimension{bounding_box.value().width(),
                               bounding_box.value().height()},
        pre_orient_dimension));
  } else if (pre_orient_dimension != buffer.dimension()) {
    // non-uniform resize.
    frame_buffer_operations.push_back(
        CropResizeOperation(0, 0, buffer.dimension(), pre_orient_dimension));
  }

  // Handle color space conversion first if the input format is RGB or RGBA,
  // because the rotation performance for RGB and RGBA formats are not optimzed
  // in libyuv.
  if (buffer.format() == FrameBuffer::Format::kRGB ||
      buffer.format() == FrameBuffer::Format::kRGBA) {
    if (output_buffer->format() != buffer.format()) {
      frame_buffer_operations.push_back(
          ConvertOperation(output_buffer->format()));
    }
    // Handle orientation conversion
    if (output_buffer->orientation() != buffer.orientation()) {
      frame_buffer_operations.push_back(
          OrientOperation(output_buffer->orientation()));
    }
  } else {
    // Handle orientation conversion first if the input format is not RGB or
    // RGBA.
    if (output_buffer->orientation() != buffer.orientation()) {
      frame_buffer_operations.push_back(
          OrientOperation(output_buffer->orientation()));
    }
    // Handle color space conversion
    if (output_buffer->format() != buffer.format()) {
      frame_buffer_operations.push_back(
          ConvertOperation(output_buffer->format()));
    }
  }

  // Execute the processing pipeline.
  if (frame_buffer_operations.empty()) {
    // Using resize to perform copy.
    TFLITE_RETURN_IF_ERROR(Resize(buffer, output_buffer));
  } else {
    TFLITE_RETURN_IF_ERROR(Execute(buffer, frame_buffer_operations, output_buffer));
  }
  return absl::OkStatus();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
