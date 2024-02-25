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
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

#include <cstdlib>
#include <cstring>
#include <vector>

// These need to be defined for stb_image.h and stb_image_write.h to include
// the actual implementations of image decoding/encoding functions.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/match.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "stb_image.h"  // from @stblib
#include "stb_image_write.h"  // from @stblib
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"

namespace tflite {
namespace task {
namespace vision {

using ::tflite::support::StatusOr;

namespace {

absl::Status CheckImageData(const ImageData& image_data) {
  if (image_data.pixel_data == nullptr) {
    return absl::InternalError(absl::StrFormat(
        "An error occurred while decoding image: %s", stbi_failure_reason()));
  }
  if (image_data.channels != 1 && image_data.channels != 3 &&
      image_data.channels != 4) {
    stbi_image_free(image_data.pixel_data);
    return absl::UnimplementedError(
        absl::StrFormat("Expected image with 1 (grayscale), 3 (RGB) or 4 "
                        "(RGBA) channels, found %d",
                        image_data.channels));
  }
  return absl::OkStatus();
}

}  // namespace

StatusOr<ImageData> DecodeImageFromFile(const std::string& file_name) {
  ImageData image_data;
  image_data.pixel_data = stbi_load(file_name.c_str(), &image_data.width,
                                    &image_data.height, &image_data.channels,
                                    /*desired_channels=*/0);
  TFLITE_RETURN_IF_ERROR(CheckImageData(image_data));
  return image_data;
}

tflite::support::StatusOr<ImageData> DecodeImageFromBuffer(
    unsigned char const* buffer, int len) {
  ImageData image_data;
  image_data.pixel_data = stbi_load_from_memory(
      buffer, len, &image_data.width, &image_data.height, &image_data.channels,
      /*desired_channels=*/0);
  TFLITE_RETURN_IF_ERROR(CheckImageData(image_data));
  return image_data;
}

absl::Status EncodeImageToPngFile(const ImageData& image_data,
                                  const std::string& image_path) {
  // Sanity check inputs.
  if (image_data.width <= 0 || image_data.height <= 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Expected positive image dimensions, found %d x %d.",
                        image_data.width, image_data.height));
  }
  if (image_data.channels != 1 && image_data.channels != 3 &&
      image_data.channels != 4) {
    return absl::UnimplementedError(
        absl::StrFormat("Expected image data with 1 (grayscale), 3 (RGB) or 4 "
                        "(RGBA) channels, found %d",
                        image_data.channels));
  }
  if (image_data.pixel_data == nullptr) {
    return absl::InvalidArgumentError(
        "Expected pixel data to be set, found nullptr.");
  }

  if (stbi_write_png(
          image_path.c_str(), image_data.width, image_data.height,
          image_data.channels, image_data.pixel_data,
          /*stride_in_bytes=*/image_data.width * image_data.channels) == 0) {
    return absl::InternalError("An error occurred while encoding image.");
  }

  return absl::OkStatus();
}

void ImageDataFree(ImageData* image) { stbi_image_free(image->pixel_data); }

tflite::support::StatusOr<std::unique_ptr<FrameBuffer>>
CreateFrameBufferFromImageData(const ImageData& image) {
  if (image.channels == 1) {
    return CreateFromGrayRawBuffer(image.pixel_data,
                                   {image.width, image.height});
  }
  if (image.channels == 3) {
    return CreateFromRgbRawBuffer(image.pixel_data,
                                  {image.width, image.height});
  } else if (image.channels == 4) {
    return CreateFromRgbaRawBuffer(image.pixel_data,
                                   {image.width, image.height});
  }
  return absl::InvalidArgumentError(
      absl::StrFormat("Expected image with 1 (grayscale), 3 (RGB) or 4 (RGBA) "
                      "channels, found %d",
                      image.channels));
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
