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
//  tensorflow_lite_support/examples/task/vision/desktop:object_detector_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --image_path=/path/to/image.jpg \
//  --output_png=/path/to/output.png

#include <iostream>
#include <limits>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/match.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/object_detector.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/detections_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/object_detector_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' object detector model.");
ABSL_FLAG(std::string, image_path, "",
          "Absolute path to the image to run detection on. The image must be "
          "RGB or RGBA (grayscale is not supported). The image EXIF "
          "orientation flag, if any, is NOT taken into account.");
ABSL_FLAG(std::string, output_png, "",
          "Absolute path to a file where to draw the detection results on top "
          "of the input image. Must have a '.png' extension.");
ABSL_FLAG(int32, max_results, 5,
          "Maximum number of detection results to display.");
ABSL_FLAG(
    float, score_threshold, std::numeric_limits<float>::lowest(),
    "Detection results with a confidence score below this value are "
    "rejected. If specified, overrides the score threshold(s) provided in the "
    "TFLite Model Metadata. Ignored otherwise.");
ABSL_FLAG(
    std::vector<std::string>, class_name_whitelist, {},
    "Comma-separated list of class names that acts as a whitelist. If "
    "non-empty, detections results whose 'class_name' is not in this list "
    "are filtered out. Mutually exclusive with 'class_name_blacklist'.");
ABSL_FLAG(std::vector<std::string>, class_name_blacklist, {},
          "Comma-separated list of class names that acts as a blacklist. If "
          "non-empty, detections results whose 'class_name' is in this list "
          "are filtered out. Mutually exclusive with 'class_name_whitelist'.");
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

namespace {
// The line thickness (in pixels) for drawing the detection results.
constexpr int kLineThickness = 3;

// The number of colors used for drawing the detection results.
constexpr int kColorMapSize = 10;

// The names of the colors used for drawing the detection results.
constexpr std::array<absl::string_view, 10> kColorMapNames = {
    "red",      "green",      "blue",      "yellow", "fuschia",
    "dark red", "dark green", "dark blue", "gray",   "black"};

// The colors used for drawing the detection results as a flattened array of
// {R,G,B} components.
constexpr uint8 kColorMapComponents[30] = {
    255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0,   255, 0, 255,
    128, 0, 0, 0, 128, 0, 0, 0, 128, 128, 128, 128, 0,   0, 0};
}  // namespace

ObjectDetectorOptions BuildOptions() {
  ObjectDetectorOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  options.set_max_results(absl::GetFlag(FLAGS_max_results));
  if (absl::GetFlag(FLAGS_score_threshold) >
      std::numeric_limits<float>::lowest()) {
    options.set_score_threshold(absl::GetFlag(FLAGS_score_threshold));
  }
  for (const std::string& class_name :
       absl::GetFlag(FLAGS_class_name_whitelist)) {
    options.add_class_name_whitelist(class_name);
  }
  for (const std::string& class_name :
       absl::GetFlag(FLAGS_class_name_blacklist)) {
    options.add_class_name_blacklist(class_name);
  }
  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return options;
}

absl::Status EncodeResultToPngFile(const DetectionResult& result,
                                   const ImageData* image) {
  for (int index = 0; index < result.detections_size(); ++index) {
    // Get bounding box as left, top, right, bottom.
    const BoundingBox& box = result.detections(index).bounding_box();
    const int left = box.origin_x();
    const int top = box.origin_y();
    const int right = box.origin_x() + box.width();
    const int bottom = box.origin_y() + box.height();
    // Get color components.
    const uint8 r = kColorMapComponents[3 * (index % kColorMapSize)];
    const uint8 g = kColorMapComponents[3 * (index % kColorMapSize) + 1];
    const uint8 b = kColorMapComponents[3 * (index % kColorMapSize) + 2];
    // Draw. Boxes might have coordinates outside of [0, w( x [0, h( so clamping
    // is applied.
    for (int y = std::max(0, top); y < std::min(image->height, bottom); ++y) {
      for (int x = std::max(0, left); x < std::min(image->width, right); ++x) {
        int pixel_index = image->channels * (image->width * y + x);
        if (x < left + kLineThickness || x > right - kLineThickness ||
            y < top + kLineThickness || y > bottom - kLineThickness) {
          image->pixel_data[pixel_index] = r;
          image->pixel_data[pixel_index + 1] = g;
          image->pixel_data[pixel_index + 2] = b;
        }
      }
    }
  }
  // Encode to PNG and return.
  TFLITE_RETURN_IF_ERROR(
      EncodeImageToPngFile(*image, absl::GetFlag(FLAGS_output_png)));
  std::cout << absl::StrFormat("Results saved to: %s\n",
                               absl::GetFlag(FLAGS_output_png));
  return absl::OkStatus();
}

void DisplayResult(const DetectionResult& result) {
  std::cout << "Results:\n";
  for (int index = 0; index < result.detections_size(); ++index) {
    std::cout << absl::StrFormat(" Detection #%d (%s):\n", index,
                                 kColorMapNames[index % kColorMapSize]);
    const Detection& detection = result.detections(index);
    const BoundingBox& box = detection.bounding_box();
    std::cout << absl::StrFormat("  Box: (x: %d, y: %d, w: %d, h: %d)\n",
                                 box.origin_x(), box.origin_y(), box.width(),
                                 box.height());
    if (detection.classes_size() == 0) {
      std::cout << "  No top-1 class available";
    } else {
      std::cout << "  Top-1 class:\n";
      const Class& classification = detection.classes(0);
      std::cout << absl::StrFormat("   index       : %d\n",
                                   classification.index());
      std::cout << absl::StrFormat("   score       : %.5f\n",
                                   classification.score());
      if (classification.has_class_name()) {
        std::cout << absl::StrFormat("   class name  : %s\n",
                                     classification.class_name());
      }
      if (classification.has_display_name()) {
        std::cout << absl::StrFormat("   display name: %s\n",
                                     classification.display_name());
      }
    }
  }
}

absl::Status Detect() {
  // Build ObjectDetector.
  const ObjectDetectorOptions& options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<ObjectDetector> object_detector,
                   ObjectDetector::CreateFromOptions(options));

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

  // Run object detection and draw results on input image.
  auto start_detect = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(DetectionResult result,
                   object_detector->Detect(*frame_buffer));
  auto end_detect = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to detect the input image on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_detect -
                                                        start_detect)
                   .count()
            << " ms" << std::endl;

  TFLITE_RETURN_IF_ERROR(EncodeResultToPngFile(result, &image));

  // Display results as text.
  DisplayResult(result);

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
  if (absl::GetFlag(FLAGS_output_png).empty()) {
    std::cerr << "Missing mandatory 'output_png' argument.\n";
    return 1;
  }
  if (!absl::EndsWithIgnoreCase(absl::GetFlag(FLAGS_output_png), ".png")) {
    std::cerr << "Argument 'output_png' must end with '.png' or '.PNG'\n";
    return 1;
  }
  if (!absl::GetFlag(FLAGS_class_name_whitelist).empty() &&
      !absl::GetFlag(FLAGS_class_name_blacklist).empty()) {
    std::cerr << "'class_name_whitelist' and 'class_name_blacklist' arguments "
                 "are mutually exclusive.\n";
    return 1;
  }

  // Run detection.
  absl::Status status = tflite::task::vision::Detect();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Detection failed: " << status.message() << "\n";
    return 1;
  }
}
