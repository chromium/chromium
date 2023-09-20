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

// Computes and displays cosine similarity between the feature vectors extracted
// on two images.
//
// Example usage:
// bazel run -c opt \
//  tensorflow_lite_support/examples/task/vision/desktop:image_embedder_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --first_image_path=/path/to/first/image.jpg \
//  --second_image_path=/path/to/second/image.jpg

#include <iostream>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/image_embedder.h"
#include "tensorflow_lite_support/cc/task/vision/proto/embeddings_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_embedder_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' image embedder model.");
ABSL_FLAG(std::string, first_image_path, "",
          "Absolute path to the first image, whose feature vector will be "
          "extracted and compared to the second image using cosine similarity. "
          "The image must be RGB or RGBA (grayscale is not supported). The "
          "image EXIF orientation flag, if any, is NOT taken into account.");
ABSL_FLAG(std::string, second_image_path, "",
          "Absolute path to the second image, whose feature vector will be "
          "extracted and compared to the first image using cosine similarity. "
          "The image must be RGB or RGBA (grayscale is not supported). The "
          "image EXIF orientation flag, if any, is NOT taken into account.");
ABSL_FLAG(bool, l2_normalize, false,
          "If true, the raw feature vectors returned by the image embedder "
          "will be normalized with L2-norm. Generally only needed if the model "
          "doesn't already contain a L2_NORMALIZATION TFLite Op.");
ABSL_FLAG(
    bool, quantize, false,
    "If true, the raw feature vectors returned by the image embedder will "
    "be quantized to 8 bit integers (uniform quantization) via post-processing "
    "before cosine similarity is computed.");
ABSL_FLAG(bool, use_coral, false,
          "If true, inference will be delegated to a connected Coral Edge TPU "
          "device.");

namespace tflite {
namespace task {
namespace vision {

namespace {
using std::chrono::microseconds;
using std::chrono::steady_clock;
using ::tflite::support::StatusOr;
}  // namespace

ImageEmbedderOptions BuildOptions() {
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  options.set_l2_normalize(absl::GetFlag(FLAGS_l2_normalize));
  options.set_quantize(absl::GetFlag(FLAGS_quantize));

  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_compute_settings()->mutable_tflite_settings()->set_delegate(
        ::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return options;
}

StatusOr<std::unique_ptr<FrameBuffer>> BuildFrameBufferFromImageData(
    const ImageData& image) {
  std::unique_ptr<FrameBuffer> frame_buffer;
  if (image.channels == 3) {
    return CreateFromRgbRawBuffer(image.pixel_data,
                                  {image.width, image.height});
  } else if (image.channels == 4) {
    return CreateFromRgbaRawBuffer(image.pixel_data,
                                   {image.width, image.height});
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Expected image with 3 (RGB) or 4 (RGBA) channels, found %d",
      image.channels));
}

absl::Status ComputeCosineSimilarity() {
  // Build ImageEmbedder.
  const ImageEmbedderOptions& options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<ImageEmbedder> image_embedder,
                   ImageEmbedder::CreateFromOptions(options));

  // Load images into FrameBuffer objects.
  TFLITE_ASSIGN_OR_RETURN(ImageData first_image,
                   DecodeImageFromFile(absl::GetFlag(FLAGS_first_image_path)));
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<FrameBuffer> first_frame_buffer,
                   BuildFrameBufferFromImageData(first_image));
  TFLITE_ASSIGN_OR_RETURN(ImageData second_image,
                   DecodeImageFromFile(absl::GetFlag(FLAGS_second_image_path)));
  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<FrameBuffer> second_frame_buffer,
                   BuildFrameBufferFromImageData(second_image));

  // Extract feature vectors.
  auto start_embed = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(const EmbeddingResult& first_embedding_result,
                   image_embedder->Embed(*first_frame_buffer));
  auto end_embed = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to embed the input image on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_embed - start_embed)
                   .count()
            << " ms" << std::endl;

  TFLITE_ASSIGN_OR_RETURN(const EmbeddingResult& second_embedding_result,
                   image_embedder->Embed(*second_frame_buffer));
  // Compute cosine similarity.
  TFLITE_ASSIGN_OR_RETURN(
      double cosine_similarity,
      ImageEmbedder::CosineSimilarity(
          image_embedder->GetEmbeddingByIndex(first_embedding_result, 0)
              .feature_vector(),
          image_embedder->GetEmbeddingByIndex(second_embedding_result, 0)
              .feature_vector()));

  // Display result.
  std::cout << absl::StrFormat("Cosine similarity: %f\n", cosine_similarity);

  // Cleanup and return.
  ImageDataFree(&first_image);
  ImageDataFree(&second_image);
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
  if (absl::GetFlag(FLAGS_first_image_path).empty()) {
    std::cerr << "Missing mandatory 'first_image_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_second_image_path).empty()) {
    std::cerr << "Missing mandatory 'second_image_path' argument.\n";
    return 1;
  }

  // Compute cosine similarity.
  absl::Status status = tflite::task::vision::ComputeCosineSimilarity();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Cosine similarity computation failed: " << status.message()
              << "\n";
    return 1;
  }
}
