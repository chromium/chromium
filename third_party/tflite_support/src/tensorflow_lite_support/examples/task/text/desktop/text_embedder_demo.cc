/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
//  tensorflow_lite_support/examples/task/text/desktop:text_embedder_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --first_sentence="first sentence" \
//  --second_sentence="second sentence"

#include <iostream>
#include <memory>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/configuration_proto_inc.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"
#include "tensorflow_lite_support/cc/task/text/proto/text_embedder_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/text_embedder.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' text embedder model.");
ABSL_FLAG(std::string, first_sentence, "",
          "First sentence, whose feature vector will be extracted and compared "
          "to the second sentence using cosine similarity.");
ABSL_FLAG(std::string, second_sentence, "",
          "Second sentence, whose feature vector will be extracted and "
          "compared to the first sentence using cosine similarity.");
ABSL_FLAG(bool, l2_normalize, false,
          "If true, the raw feature vectors returned by the image embedder "
          "will be normalized with L2-norm. Generally only needed if the model "
          "doesn't already contain a L2_NORMALIZATION TFLite Op.");
ABSL_FLAG(bool, use_coral, false,
          "If true, inference will be delegated to a connected Coral Edge TPU "
          "device.");

namespace tflite {
namespace task {
namespace text {

namespace {
using std::chrono::microseconds;
using std::chrono::steady_clock;
}  // namespace

TextEmbedderOptions BuildOptions() {
  TextEmbedderOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  if (absl::GetFlag(FLAGS_l2_normalize)) {
    options.add_embedding_options()->set_l2_normalize(true);
  }
  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }
  return options;
}

absl::Status ComputeCosineSimilarity() {
  // Build TextEmbedder.
  const TextEmbedderOptions options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(
      std::unique_ptr<TextEmbedder> text_embedder,
      TextEmbedder::CreateFromOptions(options, CreateTextOpResolver()));

  // Run search and display results.
  auto start_embed = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(processor::EmbeddingResult first_embedding,
                   text_embedder->Embed(absl::GetFlag(FLAGS_first_sentence)));
  auto end_embed = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to compute embedding for first sentence on "
            << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_embed - start_embed)
                   .count()
            << " ms" << std::endl;

  TFLITE_ASSIGN_OR_RETURN(processor::EmbeddingResult second_embedding,
                   text_embedder->Embed(absl::GetFlag(FLAGS_second_sentence)));
  // Compute cosine similarity.
  TFLITE_ASSIGN_OR_RETURN(double cosine_similarity,
                   TextEmbedder::CosineSimilarity(
                       first_embedding.embeddings(0).feature_vector(),
                       second_embedding.embeddings(0).feature_vector()));

  // Display result.
  std::cout << absl::StrFormat("Cosine similarity: %f\n", cosine_similarity);
  return absl::OkStatus();
}

}  // namespace text
}  // namespace task
}  // namespace tflite

int main(int argc, char** argv) {
  // Parse command line and perform sanity checks.
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_model_path).empty()) {
    std::cerr << "Missing mandatory 'model_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_first_sentence).empty()) {
    std::cerr << "Missing mandatory 'first_sentence' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_second_sentence).empty()) {
    std::cerr << "Missing mandatory 'second_sentence' argument.\n";
    return 1;
  }

  // Run search.
  absl::Status status = tflite::task::text::ComputeCosineSimilarity();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Cosine similarity computation failed: " << status.message()
              << "\n";
    return 1;
  }
}
