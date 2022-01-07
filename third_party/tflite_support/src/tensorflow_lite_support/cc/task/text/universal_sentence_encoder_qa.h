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
#ifndef THIRD_PARTY_TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_UNIVERSAL_SENTENCE_ENCODER_QA_H_
#define THIRD_PARTY_TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_UNIVERSAL_SENTENCE_ENCODER_QA_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"  // from @com_google_absl
#include "absl/status/status.h"            // from @com_google_absl
#include "absl/strings/str_format.h"       // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"
#include "tensorflow_lite_support/cc/task/text/proto/retrieval.pb.h"

namespace tflite {
namespace task {
namespace text {
namespace retrieval {

// QAInput and QAOutput for UniversalSentenceEncoderQA internally.
namespace internal {
struct QAInput;
struct QAOutput;
}  // namespace internal

// Creates custom op resolver for USE QA task.
std::unique_ptr<tflite_shims::ops::builtin::BuiltinOpResolver>
CreateQACustomOpResolver();

// Universal Sentence Encoder (USE) Question Answerer. The model uses USE as the
// backbone and answers a question.
class UniversalSentenceEncoderQA
    : public core::BaseTaskApi<internal::QAOutput, const internal::QAInput&> {
 public:
  using BaseTaskApi::BaseTaskApi;
  using FeatureVector = ::tflite::task::processor::FeatureVector;

  // TODO(b/198995952): add support to parameterize.
  static constexpr int kFinalEmbeddingSize = 100;

  static tflite::support::StatusOr<std::unique_ptr<UniversalSentenceEncoderQA>>
  CreateFromOption(const tflite::task::text::RetrievalOptions& options,
                   std::unique_ptr<tflite::OpResolver> resolver =
                       CreateQACustomOpResolver());

  // Retrieves output from the input by running TFLite engine.
  // Returns an error, if either query_text or responses is empty.
  tflite::support::StatusOr<RetrievalOutput> Retrieve(
      const RetrievalInput& input);

  // Encodes query from the text.
  // Returns an error, if query text is empty.
  tflite::support::StatusOr<FeatureVector> EncodeQuery(
      absl::string_view query_text);

  // Encodes response from the text and/or context.
  // Returns an error, if both text and context are empty.
  tflite::support::StatusOr<FeatureVector> EncodeResponse(
      absl::string_view response_text,
      absl::string_view response_context);

  // Calculates similarity between two encoded vectors (require same size).
  static tflite::support::StatusOr<float> Similarity(const FeatureVector& a,
                                                     const FeatureVector& b);

  // Gets top k corresponding to output response scores in descending order.
  // If k == 0, all responses are ranked.
  static std::vector<size_t> Top(const RetrievalOutput& output, size_t k = 0);

 private:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          const internal::QAInput& input) override;

  tflite::support::StatusOr<internal::QAOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors,
      const internal::QAInput& input) override;

  internal::QAOutput Run(absl::string_view query_text,
                         absl::string_view response_text,
                         absl::string_view response_context);

  std::unique_ptr<tflite::task::text::RetrievalOptions> proto_options_;
};

}  // namespace retrieval
}  // namespace text
}  // namespace task
}  // namespace tflite

#endif  // THIRD_PARTY_TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_UNIVERSAL_SENTENCE_ENCODER_QA_H_
