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

// Demostration the usage of UniversalSentenceEncoderQA.
#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/task/text/universal_sentence_encoder_qa.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"

namespace {
using tflite::task::text::CreateTextOpResolver;
using tflite::task::text::RetrievalInput;
using tflite::task::text::RetrievalOptions;
using tflite::task::text::RetrievalOutput;
using tflite::task::text::UniversalSentenceEncoderQA;
}  // namespace

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' UniversalSentenceEncoderQA model.");
ABSL_FLAG(std::string, question, "How are you feeling today?",
          "Question to ask.");
ABSL_FLAG(
    std::string, answers,
    "I'm not feeling very well.:Paris is the capital of France.:He looks good.",
    "Candidate answers seperated by `:`.");

int main(int argc, char** argv) {
  // Parse command line arguments and perform sanity checks.
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_model_path).empty()) {
    std::cerr << "Missing mandatory 'model_path' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_question).empty()) {
    std::cerr << "Missing mandatory 'question' argument.\n";
    return 1;
  }
  if (absl::GetFlag(FLAGS_answers).empty()) {
    std::cerr << "Missing mandatory 'answers' argument.\n";
    return 1;
  }

  RetrievalOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  auto status = UniversalSentenceEncoderQA::CreateFromOption(
      options, CreateTextOpResolver());
  if (!status.ok()) {
    std::cerr << "Retrieve failed: " << status.status().message() << std::endl;
    return 1;
  }

  std::unique_ptr<UniversalSentenceEncoderQA> client =
      std::move(status.value());

  // Create RetrievalInput with a query and responses.
  RetrievalInput input;
  // Set a sentence of text as the query.
  input.set_query_text(absl::GetFlag(FLAGS_question));
  // Add candidate responses, and each one contains a sentence of text. (May
  // set context too).
  for (const auto& ans : absl::StrSplit(absl::GetFlag(FLAGS_answers), ':')) {
    input.add_responses()->mutable_raw_text()->set_text(std::string(ans));
  }

  // Run inference with the Retrieve function.
  const absl::StatusOr<RetrievalOutput>& output_status =
      client->Retrieve(input);
  if (!output_status.ok()) {
    std::cerr << "Retrieve failed: " << output_status.status().message()
              << std::endl;
    return 1;
  }
  const RetrievalOutput& output = output_status.value();

  // Get top results (may set optional parameter k=? to limit top-K results).
  const std::vector<size_t>& top = client->Top(output);

  // Consume the results according to the ranking. Here we just print them out.
  std::cout << "Input questions: " << input.query_text() << std::endl;
  for (size_t k : top) {
    std::cout << "Output answers " << k << ": "
              << input.responses(k).raw_text().text()
              << " Score: " << output.response_results(k).score() << std::endl;
  }
}
