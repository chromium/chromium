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

// Example usage:
// bazel run -c opt \
//  tensorflow_lite_support/examples/task/vision/desktop:image_segmenter_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --image_path=/path/to/image.jpg \
//  --output_mask_png=/path/to/output/mask.png

#include <iostream>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/match.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/image_segmenter.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_segmenter_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/segmentations_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' image segmenter model.");
ABSL_FLAG(std::string, image_path, "",
          "Absolute path to the image to segment. The image must be RGB or "
          "RGBA (grayscale is not supported). The image EXIF orientation "
          "flag, if any, is NOT taken into account.");
ABSL_FLAG(std::string, output_mask_png, "",
          "Absolute path to the output category mask (confidence masks outputs "
          "are not supported by this tool). Must have a '.png' extension.");
ABSL_FLAG(bool, use_coral, false,
          "If true, inference will be delegated to a connected Coral Edge TPU "
          "device.");

namespace tflite {
namespace task {
namespace vision {

namespace {
using std::chrono::microseconds;
using std::chrono::steady_clock;
}  // namespace

ImageSegmenterOptions BuildOptions() {
  ImageSegmenterOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  // Confidence masks are not supported by this tool: output_type is set to
  // CATEGORY_MASK by default.

  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return options;
}

absl::Status EncodeMaskToPngFile(const SegmentationResult& result) {
  if (result.segmentation_size() != 1) {
    return absl::UnimplementedError(
        "Image segmentation models with multiple output segmentations are not "
        "supported by this tool.");
  }
  const Segmentation& segmentation = result.segmentation(0);
  // Extract raw mask data as a uint8 pointer.
  const uint8* raw_mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());

  // Create RgbImageData for the output mask.
  uint8* pixel_data = static_cast<uint8*>(
      malloc(segmentation.width() * segmentation.height() * 3 * sizeof(uint8)));
  ImageData mask = {.pixel_data = pixel_data,
                    .width = segmentation.width(),
                    .height = segmentation.height(),
                    .channels = 3};

  // Populate RgbImageData from the raw mask and ColoredLabel-s.
  for (int i = 0; i < segmentation.width() * segmentation.height(); ++i) {
    Segmentation::ColoredLabel colored_label =
        segmentation.colored_labels(raw_mask[i]);
    pixel_data[3 * i] = colored_label.r();
    pixel_data[3 * i + 1] = colored_label.g();
    pixel_data[3 * i + 2] = colored_label.b();
  }

  // Encode mask as PNG.
  TFLITE_RETURN_IF_ERROR(
      EncodeImageToPngFile(mask, absl::GetFlag(FLAGS_output_mask_png)));
  std::cout << absl::StrFormat("Category mask saved to: %s\n",
                               absl::GetFlag(FLAGS_output_mask_png));

  // Cleanup and return.
  ImageDataFree(&mask);
  return absl::OkStatus();
}

absl::Status DisplayColorLegend(const SegmentationResult& result) {
  if (result.segmentation_size() != 1) {
    return absl::UnimplementedError(
        "Image segmentation models with multiple output segmentations are not "
        "supported by this tool.");
  }
  const Segmentation& segmentation = result.segmentation(0);
  const int num_labels = segmentation.colored_labels_size();

  std::cout << "Color Legend:\n";
  for (int index = 0; index < num_labels; ++index) {
    Segmentation::ColoredLabel colored_label =
        segmentation.colored_labels(index);
    std::cout << absl::StrFormat(" (r: %03d, g: %03d, b: %03d):\n",
                                 colored_label.r(), colored_label.g(),
                                 colored_label.b());
    std::cout << absl::StrFormat("  index       : %d\n", index);
    if (colored_label.has_class_name()) {
      std::cout << absl::StrFormat("  class name  : %s\n",
                                   colored_label.class_name());
    }
    if (colored_label.has_display_name()) {
      std::cout << absl::StrFormat("  display name: %s\n",
                                   colored_label.display_name());
    }
  }
  std::cout << "Tip: use a color picker on the output PNG file to inspect the "
               "output mask with this legend.\n";

  return absl::OkStatus();
}

absl::Status Segment() {
  // Build ImageClassifier.
  const ImageSegmenterOptions& options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<ImageSegmenter> image_segmenter,
                   ImageSegmenter::CreateFromOptions(options));

  // Load image in a FrameBuffer.
  TFLITE_ASSIGN_OR_RETURN(ImageData image,
                   DecodeImageFromFile(absl::GetFlag(FLAGS_image_path)));
  std::unique_ptr<FrameBuffer> frame_buffer;
  if (image.channels == 3) {
    frame_buffer =
        CreateFromRgbRawBuffer(image.pixel_data, {image.width, image.height});
  } else if (image.channels == 4) {
    frame_buffer =
        CreateFromRgbaRawBuffer(image.pixel_data, {image.width, image.height});
  } else {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Expected image with 3 (RGB) or 4 (RGBA) channels, found %d",
        image.channels));
  }

  // Run segmentation and save category mask.
  auto start_segment = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(SegmentationResult result,
                   image_segmenter->Segment(*frame_buffer));
  auto end_segment = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to segment the input image on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_segment -
                                                        start_segment)
                   .count()
            << " ms" << std::endl;

  TFLITE_RETURN_IF_ERROR(EncodeMaskToPngFile(result));

  // Display the legend.
  TFLITE_RETURN_IF_ERROR(DisplayColorLegend(result));

  // Cleanup and return.
  ImageDataFree(&image);
  return absl::OkStatus();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite

int main(int argc, char** argv) {
  // Parse command line arguments and perform sanity checks.
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_model_path).empty()) {
    std::cerr << "Missing mandatory 'model_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_image_path).empty()) {
    std::cerr << "Missing mandatory 'image_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_output_mask_png).empty()) {
    std::cerr << "Missing mandatory 'output_mask_png' argument.\n";
    return 1;
  }
  if (!absl::EndsWithIgnoreCase(absl::GetFlag(FLAGS_output_mask_png), ".png")) {
    std::cerr << "Argument 'output_mask_png' must end with '.png' or '.PNG'\n";
    return 1;
  }

  // Run segmentation.
  absl::Status status = tflite::task::vision::Segment();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Segmentation failed: " << status.message() << "\n";
    return 1;
  }
}
