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
//  tensorflow_lite_support/examples/task/text/desktop:text_searcher_demo \
//  -- \
//  --model_path=/path/to/model.tflite \
//  --index_path=/path/to/index.ldb \
//  --input_sentence="your_input"

#include <iostream>
#include <memory>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/configuration_proto_inc.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options.pb.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/text/proto/text_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/text_searcher.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' text embedder model.");
ABSL_FLAG(std::string, index_path, "",
          "Absolute path to the index to search into. Mandatory only if the "
          "index is not attached to the output tensor metadata of the embedder "
          "model as an AssociatedFile with type SCANN_INDEX_FILE.");
ABSL_FLAG(std::string, input_sentence, "",
          "Input sentence whose nearest-neighbors to search for in the index.");
ABSL_FLAG(int32, max_results, 5,
          "Maximum number of nearest-neghbors to display.");
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

TextSearcherOptions BuildOptions() {
  TextSearcherOptions options;
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

void DisplayResults(const processor::SearchResult& result) {
  std::cout << "Results:\n";
  for (int rank = 0; rank < result.nearest_neighbors_size(); ++rank) {
    const auto& neighbor = result.nearest_neighbors(rank);
    std::cout << absl::StrFormat(" Rank#%d:\n", rank);
    std::cout << absl::StrFormat("  metadata: %s\n", neighbor.metadata());
    std::cout << absl::StrFormat("  distance: %.5f\n", neighbor.distance());
  }
}

absl::Status Search() {
  // Build TextSearcher.
  const TextSearcherOptions options = BuildOptions();
  TFLITE_ASSIGN_OR_RETURN(
      std::unique_ptr<TextSearcher> text_searcher,
      TextSearcher::CreateFromOptions(options, CreateTextOpResolver()));

  // Run search and display results.
  auto start_search = steady_clock::now();
  TFLITE_ASSIGN_OR_RETURN(processor::SearchResult result,
                   text_searcher->Search(absl::GetFlag(FLAGS_input_sentence)));
  auto end_search = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to search the input text on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_search -
                                                        start_search)
                   .count()
            << " ms" << std::endl;

  DisplayResults(result);

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
  if (absl::GetFlag(FLAGS_input_sentence).empty()) {
    std::cerr << "Missing mandatory 'input_sentence' argument.\n";
    return 1;
  }

  // Run search.
  absl::Status status = tflite::task::text::Search();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Search failed: " << status.message() << "\n";
    return 1;
  }
}
