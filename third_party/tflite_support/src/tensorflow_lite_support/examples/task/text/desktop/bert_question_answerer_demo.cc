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
#include <iostream>
#include <limits>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/flags/parse.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"

ABSL_FLAG(std::string, model_path, "",
          "Absolute path to the '.tflite' bert question answerer model.");
ABSL_FLAG(std::string, question, "", "Question to ask.");
ABSL_FLAG(std::string, context, "",
          "Context the asked question is based upon.");
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

absl::Status Answer() {
  BertQuestionAnswererOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      absl::GetFlag(FLAGS_model_path));
  if (absl::GetFlag(FLAGS_use_coral)) {
    options.mutable_base_options()
        ->mutable_compute_settings()
        ->mutable_tflite_settings()
        ->set_delegate(::tflite::proto::Delegate::EDGETPU_CORAL);
  }

  TFLITE_ASSIGN_OR_RETURN(std::unique_ptr<QuestionAnswerer> answerer,
                   BertQuestionAnswerer::CreateFromOptions(options));

  auto start_answer = steady_clock::now();
  std::vector<QaAnswer> answers = answerer->Answer(
      absl::GetFlag(FLAGS_context), absl::GetFlag(FLAGS_question));
  auto end_answer = steady_clock::now();
  std::string delegate =
      absl::GetFlag(FLAGS_use_coral) ? "Coral Edge TPU" : "CPU";
  std::cout << "Time cost to answer the input question on " << delegate << ": "
            << std::chrono::duration<float, std::milli>(end_answer -
                                                        start_answer)
                   .count()
            << " ms" << std::endl;

  for (int i = 0; i < answers.size(); ++i) {
    const QaAnswer& answer = answers[i];
    std::cout << absl::StrFormat(
        "answer[%d]: '%s'\n    logit: '%.5f, start_index: %d, end_index: %d\n",
        i, answer.text, answer.pos.logit, answer.pos.start, answer.pos.end);
  }

  return absl::OkStatus();
}

}  // namespace text
}  // namespace task
}  // namespace tflite

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
  if (absl::GetFlag(FLAGS_context).empty()) {
    std::cerr << "Missing mandatory 'context' argument.\n";
    return 1;
  }
  // Run the answerer.
  absl::Status status = tflite::task::text::Answer();
  if (status.ok()) {
    return 0;
  } else {
    std::cerr << "Answer failed: " << status.message() << "\n";
    return 1;
  }
}
