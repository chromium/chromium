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
#include <string>
#include <utility>

#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/text/proto/retrieval.pb.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
namespace tflite {
namespace task {
namespace text {

namespace {
using ::testing::ElementsAreArray;
using ::tflite::support::EqualsProto;
using ::tflite::support::StatusOr;
using ::tflite::support::proto::TextFormat;
using FeatureVector = UniversalSentenceEncoderQA::FeatureVector;

constexpr char kTestUseQaModelDir[] =
    "/tensorflow_lite_support/cc/test/testdata/task/text/"
    "universal_sentence_encoder_qa_with_metadata.tflite";

// Example to measure inference value.
constexpr char kQuery[] = "When is Father's Day?";
constexpr char kResponse[] = "In the US, it falls on the third Sunday in June.";
constexpr char kContext[] =
    "Father's Day is a celebration honoring fathers and celebrating "
    "fatherhood, paternal bonds, and the influence of fathers in "
    "society.";
constexpr float kExpectedDot = 16.221f;  // Ground truth value of the model.
constexpr float kThreshold = 0.001f;

// Example to compare the best answer.
constexpr char kQueryComp[] = "How are you feeling today?";
constexpr char kResponseComp0[] = "I'm not feeling very well.";  // Best answer.
constexpr char kResponseComp1[] = "Beijing is the capital of China.";
constexpr char kResponseComp2[] = "He looks good.";
constexpr char kInputProto[] = R"(
  query_text: "How are you feeling today?"
  responses: {
    raw_text: { text: "I'm not feeling very well." }
  }
  responses: {
    raw_text: { text: "Beijing is the capital of China." }
  }
  responses: {
    raw_text: { text: "He looks good." }
  }
)";
const size_t kExpectedTop[] = {0, 2, 1};
const float kExpectedScores[] = {14.9595, 7.2148, 8.8094};

class UniversalSentenceEncoderQATest : public tflite::testing::Test {
 public:
  UniversalSentenceEncoderQATest() {
    // Load model file, and create qa client.
    const auto filename =
        JoinPath("./" /*test src dir*/, kTestUseQaModelDir);
    RetrievalOptions options;
    options.mutable_base_options()->mutable_model_file()->set_file_name(
        filename);
    auto status = UniversalSentenceEncoderQA::CreateFromOption(
        options, CreateTextOpResolver());
    if (status.ok()) {
      qa_client_ = std::move(status.value());
    }
  }

 protected:
  std::unique_ptr<UniversalSentenceEncoderQA> qa_client_;
};

TEST_F(UniversalSentenceEncoderQATest, TestEncodeQuery) {
  ASSERT_TRUE(qa_client_ != nullptr);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto encoded_question,
                       qa_client_->EncodeQuery(kQuery));
  EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
            encoded_question.value_float_size());

  // Test predoncition.
  EXPECT_FALSE(qa_client_->EncodeQuery("").ok());
}

TEST_F(UniversalSentenceEncoderQATest, TestEncodeResponse) {
  ASSERT_TRUE(qa_client_ != nullptr);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto encoded_response,
                       qa_client_->EncodeResponse(kResponse, kContext));
  EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
            encoded_response.value_float_size());

  // Test predoncition.
  EXPECT_FALSE(qa_client_->EncodeResponse("", "").ok());
}

TEST_F(UniversalSentenceEncoderQATest, TestSimilarity) {
  ASSERT_TRUE(qa_client_ != nullptr);
  FeatureVector v1, v2;
  ASSERT_TRUE(
      TextFormat::ParseFromString("value_float: 1 value_float: 2", &v1));
  ASSERT_TRUE(
      TextFormat::ParseFromString("value_float: 1 value_float: 2", &v2));
  SUPPORT_ASSERT_OK_AND_ASSIGN(float sim, qa_client_->Similarity(v1, v2));
  EXPECT_EQ(sim, 5.0f);
}

TEST_F(UniversalSentenceEncoderQATest, TestSimilarityNotSameSize) {
  ASSERT_TRUE(qa_client_ != nullptr);
  FeatureVector v1, v2;
  ASSERT_TRUE(TextFormat::ParseFromString("value_float: 1", &v1));
  ASSERT_TRUE(
      TextFormat::ParseFromString("value_float: 1 value_float: 2", &v2));
  const auto& sim = qa_client_->Similarity(v1, v2);
  EXPECT_FALSE(sim.ok());
}

TEST_F(UniversalSentenceEncoderQATest, TestTop) {
  ASSERT_TRUE(qa_client_ != nullptr);
  RetrievalOutput output;
  ASSERT_TRUE(TextFormat::ParseFromString(R"(
      query_encoding { value_float: 1 value_float: 2 }
      response_results {
        encoding { value_float: 1 value_float: 1 }
        score: 3
      }
      response_results {
        encoding { value_float: 1 value_float: 2 }
        score: 5
      }
    )",
                                          &output));

  const auto& top_1 = qa_client_->Top(output, 1);
  EXPECT_THAT(top_1, ElementsAreArray({1}));

  const auto& top_all = qa_client_->Top(output);
  EXPECT_THAT(top_all, ElementsAreArray({1, 0}));

  const size_t large_k = 999;
  const auto& top_large_k = qa_client_->Top(output, large_k);
  EXPECT_THAT(top_large_k, ElementsAreArray({1, 0}));
}

TEST_F(UniversalSentenceEncoderQATest, TestRetrieve) {
  ASSERT_TRUE(qa_client_ != nullptr);
  RetrievalInput input;
  ASSERT_TRUE(TextFormat::ParseFromString(kInputProto, &input));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& output, qa_client_->Retrieve(input));

  // Checks query.
  EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
            output.query_encoding().value_float_size());

  // Checks responses.
  const int expected_size = 3;
  EXPECT_EQ(expected_size, output.response_results_size());
  for (size_t i = 0; i < expected_size; ++i) {
    const auto& result = output.response_results(i);
    EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
              result.encoding().value_float_size());
    EXPECT_TRUE(result.has_score());
    EXPECT_NEAR(kExpectedScores[i], result.score(), kThreshold);
  }

  // Checks top.
  const auto& top = qa_client_->Top(output);
  EXPECT_THAT(top, ElementsAreArray(kExpectedTop));
}

TEST_F(UniversalSentenceEncoderQATest, TestRetrieveCheckPrecondition) {
  ASSERT_TRUE(qa_client_ != nullptr);
  RetrievalInput no_response;
  ASSERT_TRUE(TextFormat::ParseFromString(R"(query_text: "hi")", &no_response));
  EXPECT_FALSE(qa_client_->Retrieve(no_response).ok());

  RetrievalInput no_query;
  ASSERT_TRUE(TextFormat::ParseFromString(R"(
    responses: {
      raw_text: { text: "I'm not feeling very well." }
    })",
                                          &no_query));
  EXPECT_FALSE(qa_client_->Retrieve(no_query).ok());
}

TEST_F(UniversalSentenceEncoderQATest, TestRetrieveWithEncoding) {
  ASSERT_TRUE(qa_client_ != nullptr);
  RetrievalInput input;
  input.set_query_text(kQueryComp);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& query, qa_client_->EncodeQuery(kQueryComp));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& resp0,
                       qa_client_->EncodeResponse(kResponseComp0, ""));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& resp1,
                       qa_client_->EncodeResponse(kResponseComp1, ""));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& resp2,
                       qa_client_->EncodeResponse(kResponseComp2, ""));
  *input.mutable_responses()->Add()->mutable_text_encoding() = resp0;
  *input.mutable_responses()->Add()->mutable_text_encoding() = resp1;
  *input.mutable_responses()->Add()->mutable_text_encoding() = resp2;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto& output, qa_client_->Retrieve(input));

  // Check query.
  EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
            output.query_encoding().value_float_size());
  EXPECT_THAT(output.query_encoding(), EqualsProto(query));

  // Check responses.
  const std::vector<FeatureVector> expected_vectors = {resp0, resp1, resp2};
  const int expected_size = 3;
  EXPECT_EQ(expected_size, output.response_results_size());
  for (size_t i = 0; i < expected_size; ++i) {
    const auto& result = output.response_results(i);
    EXPECT_EQ(UniversalSentenceEncoderQA::kFinalEmbeddingSize,
              result.encoding().value_float_size());
    EXPECT_TRUE(result.has_score());
    // Expect encoded vector to be kept in the result.
    EXPECT_THAT(result.encoding(), EqualsProto(expected_vectors[i]));
    EXPECT_NEAR(kExpectedScores[i], result.score(), kThreshold);
  }

  // Checks top.
  const auto& top = qa_client_->Top(output);
  EXPECT_THAT(top, ElementsAreArray(kExpectedTop));
}

}  // namespace
}  // namespace text
}  // namespace task
}  // namespace tflite
