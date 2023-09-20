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

// TODO(b/226312796): add this to the README file.
// Example usage:
// bazel run -c opt \
//  tensorflow_lite_support/examples/task/vision/desktop:image_searcher_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --index_path=/path/to/index.ldb \
//  --image_path=/path/to/image.jpg

#include <iostream>
#include <memory>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options.pb.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/vision/image_searcher.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' image embedder model.");
ABSL_FLAG(std::string, index_path, "",
          "Absolute path to the index to search into. Mandatory only if the "
          "index is not attached to the output tensor metadata of the embedder "
          "model as an AssociatedFile with type SCANN_INDEX_FILE.");
ABSL_FLAG(std::string, image_path, "",
          "Absolute path to the image to search. The image must be RGB or "
          "RGBA (grayscale is not supported). The image EXIF orientation "
          "flag, if any, is NOT taken into account.");
ABSL_FLAG(int32, max_results, 5,
          "Maximum number of nearest-neighbor results to display.");
ABSL_FLAG(bool, l2_normalize, false,
          "If true, the raw feature vectors returned by the image embedder "
          "will be normalized with L2-norm. Generally only needed if the model "
          "doesn't already contain a L2_NORMALIZATION TFLite Op.");
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

ImageSearcherOptions BuildOptions() {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  if (absl::GetFlag(FLAGS_l2_normalize)) {
    options.mutable_embedding_options()->set_l2_normalize(true);
  }
  if (!absl::GetFlag(FLAGS_index_path).empty()) {
    options.mutable_search_options()->mutable_index_file()->set_file_name(
        absl::GetFlag(FLAGS_index_path));
  }
  options.mutable_search_options()->set_max_results(
      absl::GetFlag(FLAGS_max_results));
  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return options;
}

void DisplayResult(const processor::SearchResult& result) {
  std::cout << "Results:\n";
  for (int rank = 0; rank < result.nearest_neighbors_size(); ++rank) {
    const auto& neighbor = result.nearest_neighbors(rank);
    std::cout << absl::StrFormat(" Rank#%d:\n", rank);
    std::cout << absl::StrFormat("  metadata: %s\n", neighbor.metadata());
    std::cout << absl::StrFormat("  distance: %.5f\n", neighbor.distance());
  }
}

absl::Status Search() {
  // Build ImageSearcher.
  const ImageSearcherOptions options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<ImageSearcher> image_searcher,
                   ImageSearcher::CreateFromOptions(options));

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

  // Run search and display results.
  auto start_search = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(processor::SearchResult result,
                   image_searcher->Search(*frame_buffer));
  auto end_search = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to search the input image on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_search -
                                                        start_search)
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
  // Parse command line and perform sanity checks.
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_model_path).empty()) {
    std::cerr << "Missing mandatory 'model_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_image_path).empty()) {
    std::cerr << "Missing mandatory 'image_path' argument.\n";
    return 1;
  }

  // Run search.
  absl::Status status = tflite::task::vision::Search();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Search failed: " << status.message() << "\n";
    return 1;
  }
}
