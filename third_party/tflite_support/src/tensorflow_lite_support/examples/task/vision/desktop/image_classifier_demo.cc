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
//  tensorflow_lite_support/examples/task/vision/desktop:image_classifier_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --image_path=/path/to/image.jpg

#include <iostream>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "tensorflow_lite_support/cc/task/vision/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_classifier_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' image classifier model.");
ABSL_FLAG(std::string, image_path, "",
          "Absolute path to the image to classify. The image must be RGB or "
          "RGBA (grayscale is not supported). The image EXIF orientation "
          "flag, if any, is NOT taken into account.");
ABSL_FLAG(int32, max_results, 5,
          "Maximum number of classification results to display.");
ABSL_FLAG(float, score_threshold, 0,
          "Classification results with a confidence score below this value are "
          "rejected. If >= 0, overrides the score threshold(s) provided in the "
          "TFLite Model Metadata. Ignored otherwise.");
ABSL_FLAG(
    std::vector<std::string>, class_name_whitelist, {},
    "Comma-separated list of class names that acts as a whitelist. If "
    "non-empty, classification results whose 'class_name' is not in this list "
    "are filtered out. Mutually exclusive with 'class_name_blacklist'.");
ABSL_FLAG(
    std::vector<std::string>, class_name_blacklist, {},
    "Comma-separated list of class names that acts as a blacklist. If "
    "non-empty, classification results whose 'class_name' is in this list "
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

ImageClassifierOptions BuildOptions() {
  ImageClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  options.set_max_results(absl::GetFlag(FLAGS_max_results));
  if (absl::GetFlag(FLAGS_score_threshold) >= 0) {
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

void DisplayResult(const ClassificationResult& result) {
  std::cout << "Results:\n";
  for (int head = 0; head < result.classifications_size(); ++head) {
    if (result.classifications_size() > 1) {
      std::cout << absl::StrFormat(" Head index %d:\n", head);
    }
    const Classifications& classifications = result.classifications(head);
    for (int rank = 0; rank < classifications.classes_size(); ++rank) {
      const Class& classification = classifications.classes(rank);
      std::cout << absl::StrFormat("  Rank #%d:\n", rank);
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

absl::Status Classify() {
  // Build ImageClassifier.
  const ImageClassifierOptions& options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<ImageClassifier> image_classifier,
                   ImageClassifier::CreateFromOptions(options));

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

  // Run classification and display results.
  auto start_classify = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(ClassificationResult result,
                   image_classifier->Classify(*frame_buffer));
  auto end_classify = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to classify the input image on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_classify -
                                                        start_classify)
                   .count()
            << " ms" << std::endl;

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
  if (!absl::GetFlag(FLAGS_class_name_whitelist).empty() &&
      !absl::GetFlag(FLAGS_class_name_blacklist).empty()) {
    std::cerr << "'class_name_whitelist' and 'class_name_blacklist' arguments "
                 "are mutually exclusive.\n";
    return 1;
  }

  // Run classification.
  absl::Status status = tflite::task::vision::Classify();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Classification failed: " << status.message() << "\n";
    return 1;
  }
}
