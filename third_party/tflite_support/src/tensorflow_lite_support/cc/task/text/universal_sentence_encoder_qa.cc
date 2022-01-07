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
#include "tensorflow_lite_support/cc/task/text/universal_sentence_encoder_qa.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"  // from @com_google_absl
#include "absl/status/status.h"            // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/text/proto/retrieval.pb.h"

namespace tflite {
namespace ops {
namespace custom {
TfLiteRegistration* Register_SENTENCEPIECE_TOKENIZER();
TfLiteRegistration* Register_RAGGED_TENSOR_TO_TENSOR();
}  // namespace custom
}  // namespace ops
}  // namespace tflite

namespace tflite {
namespace task {
namespace text {
namespace retrieval {

using ::absl::Status;
using ::absl::StatusCode;
using internal::QAInput;
using internal::QAOutput;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::FindTensorByName;
using ::tflite::task::core::PopulateTensor;
using ::tflite::task::core::PopulateVectorToRepeated;
using ::tflite::task::core::TaskAPIFactory;
using FeatureVector = UniversalSentenceEncoderQA::FeatureVector;

namespace {
constexpr char kQueryTextTensorName[] = "inp_text";
constexpr char kResponseTextTensorName[] = "res_text";
constexpr char kResponseContextTensorName[] = "res_context";
constexpr char kQueryEncodingTensorName[] = "query_encoding";
constexpr char kResponseEncodingTensorName[] = "response_encoding";

// Sanity check for options to ensure required fields.
absl::Status SanityCheckOptions(const RetrievalOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Missing mandatory `base_options` field",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

// Copy vector from model output.
inline absl::Status CopyVector(const TfLiteTensor* src, FeatureVector* target) {
  return PopulateVectorToRepeated(src, target->mutable_value_float());
}

// Dot product of two vectors. Returns error status if size is mismatched.
template <class TCollection, class T = float>
tflite::support::StatusOr<T> Dot(const TCollection& a, const TCollection& b) {
  if (a.size() != b.size()) {
    return Status(
        StatusCode::kInvalidArgument,
        absl::StrFormat("mismatched vector size %d != %d", a.size(), b.size()));
  }
  auto dist = T();
  for (size_t i = 0; i < a.size(); ++i) {
    dist += T(a[i]) * T(b[i]);
  }
  return dist;
}
}  // namespace

namespace internal {
struct QAInput {
  std::string query_text;
  std::string response_text;
  std::string response_context;
};

struct QAOutput {
  // Directly populate from raw tensor pointers to avoid extra copy.
  const TfLiteTensor* query_encoding;     // not owned.
  const TfLiteTensor* response_encoding;  // not owned.
};
}  // namespace internal

// Creates custom op resolver for USE QA task.
std::unique_ptr<tflite_shims::ops::builtin::BuiltinOpResolver>
CreateQACustomOpResolver() {
  auto resolver =
      absl::make_unique<tflite_shims::ops::builtin::BuiltinOpResolver>();
  resolver->AddCustom(
      "TFSentencepieceTokenizeOp",
      ::tflite::ops::custom::Register_SENTENCEPIECE_TOKENIZER());
  resolver->AddCustom(
      "RaggedTensorToTensor",
      ::tflite::ops::custom::Register_RAGGED_TENSOR_TO_TENSOR());
  return resolver;
}

constexpr int UniversalSentenceEncoderQA::kFinalEmbeddingSize;

StatusOr<RetrievalOutput> UniversalSentenceEncoderQA::Retrieve(
    const RetrievalInput& input) {
  if (input.query_text().empty()) {
    return Status(StatusCode::kInvalidArgument, "query text cannot be empty.");
  }
  if (input.responses().empty()) {
    return Status(StatusCode::kInvalidArgument, "responses cannot be empty.");
  }

  RetrievalOutput output;
  // Run inference.
  // (1) Query is only encoded for once.
  // (2) If responses are raw text, run model to get encoded vectors; otherwise,
  //     the encoded vector is kept from the input when given.
  for (size_t i = 0; i < input.responses_size(); ++i) {
    const auto& resp = input.responses(i);

    if (resp.has_raw_text()) {
      // If response is in th raw text, encode both query and response.
      const auto out = Run(input.query_text(), resp.raw_text().text(),
                           resp.raw_text().context());

      // Only encode query for the first time.
      if (i == 0) {
        RETURN_IF_ERROR(
            CopyVector(out.query_encoding, output.mutable_query_encoding()));
      }

      // For each answer, set the response result.
      auto r = output.mutable_response_results()->Add();
      RETURN_IF_ERROR(CopyVector(out.response_encoding, r->mutable_encoding()));
    } else {
      // If response is already encoded, encode query only and keep response
      // encoding.

      // Only encode query for the first time.
      if (i == 0) {
        const auto& q = EncodeQuery(input.query_text());
        *output.mutable_query_encoding() = q.value();
      }

      // For each answer, set the response result from text_encoding
      auto r = output.mutable_response_results()->Add();
      *r->mutable_encoding() = resp.text_encoding();
    }
  }

  // Calculate scores.
  for (size_t i = 0; i < output.response_results_size(); ++i) {
    auto* r = output.mutable_response_results(i);
    // TODO(tianlin): For a large size of results, it is more efficient to use
    // matrix multiplication.
    const auto& score = Similarity(output.query_encoding(), r->encoding());
    if (!score.ok()) {
      return score.status();
    }
    r->set_score(score.value());
  }
  return output;
}

StatusOr<FeatureVector> UniversalSentenceEncoderQA::EncodeQuery(
    absl::string_view query_text) {
  if (query_text.empty()) {
    return Status(StatusCode::kInvalidArgument, "query text cannot be empty.");
  }

  const auto& output = Run(query_text, "", "");
  FeatureVector v;
  RETURN_IF_ERROR(CopyVector(output.query_encoding, &v));
  return v;
}

StatusOr<FeatureVector> UniversalSentenceEncoderQA::EncodeResponse(
    absl::string_view response_text,
    absl::string_view response_context) {
  if (response_text.empty() && response_context.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "either response text or context should be set to non-empty.");
  }

  const auto& output = Run("", response_text, response_context);
  FeatureVector v;
  RETURN_IF_ERROR(CopyVector(output.response_encoding, &v));
  return v;
}

StatusOr<float> UniversalSentenceEncoderQA::Similarity(const FeatureVector& a,
                                                       const FeatureVector& b) {
  const auto& av = a.value_float();
  const auto& bv = b.value_float();
  return Dot(av, bv);
}

std::vector<size_t> UniversalSentenceEncoderQA::Top(
    const RetrievalOutput& output,
    size_t k) {
  // Ensure k in [0, total_size).
  // If k == 0, it means that all outputs are ranked.
  if (k == 0) {
    k = output.response_results_size();
  } else {
    k = std::min(k, size_t(output.response_results_size()));
  }

  std::vector<size_t> pos(output.response_results_size());
  for (size_t i = 0; i < output.response_results_size(); ++i) {
    pos[i] = i;
  }
  const auto greater_score = [&output](size_t i, size_t j) {
    return output.response_results(i).score() >
           output.response_results(j).score();
  };
  std::partial_sort(pos.begin(), pos.begin() + k, pos.end(), greater_score);

  // Return sorted.
  return std::vector<size_t>(pos.begin(), pos.begin() + k);
}

Status UniversalSentenceEncoderQA::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const QAInput& input) {
  auto* input_tensor_metadatas =
      GetMetadataExtractor()->GetInputTensorMetadata();
  TfLiteTensor* query_text_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kQueryTextTensorName)
          : input_tensors[0];
  TfLiteTensor* response_text_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kResponseTextTensorName)
          : input_tensors[2];
  TfLiteTensor* response_context_tensor =
      input_tensor_metadatas
          ? FindTensorByName(input_tensors, input_tensor_metadatas,
                             kResponseContextTensorName)
          : input_tensors[1];

  RETURN_IF_ERROR(PopulateTensor(input.query_text, query_text_tensor));
  RETURN_IF_ERROR(PopulateTensor(input.response_text, response_text_tensor));
  RETURN_IF_ERROR(
      PopulateTensor(input.response_context, response_context_tensor));

  return absl::OkStatus();
}

StatusOr<QAOutput> UniversalSentenceEncoderQA::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const QAInput& /*input*/) {
  auto* output_tensor_metadatas =
      GetMetadataExtractor()->GetOutputTensorMetadata();

  const TfLiteTensor* output_query_encoding_tensor =
      output_tensor_metadatas
          ? FindTensorByName(output_tensors, output_tensor_metadatas,
                             kQueryEncodingTensorName)
          : output_tensors[0];
  const TfLiteTensor* output_response_encoding_tensor =
      output_tensor_metadatas
          ? FindTensorByName(output_tensors, output_tensor_metadatas,
                             kResponseEncodingTensorName)
          : output_tensors[1];

  QAOutput output;
  output.query_encoding = output_query_encoding_tensor;
  output.response_encoding = output_response_encoding_tensor;
  return output;
}

internal::QAOutput UniversalSentenceEncoderQA::Run(
    absl::string_view query_text,
    absl::string_view response_text,
    absl::string_view response_context) {
  QAInput input;
  input.query_text = query_text;
  input.response_text = response_text;
  input.response_context = response_context;
  return Infer(input).value();
}

StatusOr<std::unique_ptr<UniversalSentenceEncoderQA>>
UniversalSentenceEncoderQA::CreateFromOption(
    const RetrievalOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the duration of this
  // created object.
  auto options_copy = absl::make_unique<RetrievalOptions>(options);

  ASSIGN_OR_RETURN(
      auto encoder,
      TaskAPIFactory::CreateFromBaseOptions<UniversalSentenceEncoderQA>(
          &options_copy->base_options(), std::move(resolver)));
  encoder->proto_options_ = std::move(options_copy);
  return std::move(encoder);
}

}  // namespace retrieval
}  // namespace text
}  // namespace task
}  // namespace tflite
