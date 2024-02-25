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

#include "tensorflow_lite_support/cc/task/text/text_embedder.h"

#include <iostream>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace text {
namespace {

using ::tflite::support::StatusOr;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/text/";
constexpr char kMobileBert[] = "mobilebert_embedding_with_metadata.tflite";
// Embedding model with regex preprocessing and one embedding output.
constexpr char kRegexOneEmbeddingModel[] =
    "regex_one_embedding_with_metadata.tflite";
// Embedding model with regex preprocessing and two embedding outputs.
constexpr char kRegexTwoEmbeddingsModel[] =
    "regex_two_embeddings_with_metadata.tflite";
// Embedding model based on Universal Sentence Encoder, with two outputs.
constexpr char kUniversalSentenceEndoder[] =
    "universal_sentence_encoder_qa_with_metadata.tflite";
constexpr int kNumberOfOutputLayers = 1;
constexpr float kValueDiffTolerance = 1e-4;
// Tolerancy for cosine similarity evaluation.
constexpr double kSimilarityTolerancy = 1e-6;

class CreateFromOptionsTest : public tflite::testing::Test {};

TextEmbedderOptions GetBasicOptions(absl::string_view model_name) {
  TextEmbedderOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, model_name));
  return options;
}

TEST_F(CreateFromOptionsTest, SucceedsWithMobileBertWithoutEmbeddingOptions) {
  // Mobilebert model.
  TextEmbedderOptions options0 = GetBasicOptions(kMobileBert);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options0));

  // Regex model with one embedding output.
  TextEmbedderOptions options1 = GetBasicOptions(kRegexOneEmbeddingModel);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options1));

  // Regex model with two embedding output.
  TextEmbedderOptions options2 = GetBasicOptions(kRegexTwoEmbeddingsModel);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options2));
}

TEST_F(CreateFromOptionsTest, SucceedsWithSingleEmbeddingOptions) {
  // Mobilebert model.
  TextEmbedderOptions options = GetBasicOptions(kMobileBert);
  // Single embedding options means all head get the same option.
  options.add_embedding_options();
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options));

  // Regex model with one embedding output.
  TextEmbedderOptions options1 = GetBasicOptions(kRegexOneEmbeddingModel);
  // Single embedding options means all head get the same option.
  options1.add_embedding_options();
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options1));

  // Regex model with two embedding output.
  TextEmbedderOptions options2 = GetBasicOptions(kRegexTwoEmbeddingsModel);
  // Single embedding options means all head get the same option.
  options2.add_embedding_options();
  SUPPORT_ASSERT_OK(TextEmbedder::CreateFromOptions(options2));
}

TEST_F(CreateFromOptionsTest, FailsWithIncorrectNumberOfHeadOptions) {
  TextEmbedderOptions options = GetBasicOptions(kMobileBert);
  // The model has only 1 head, so it can't take two embedding options.
  options.add_embedding_options();
  options.add_embedding_options();

  StatusOr<std::unique_ptr<TextEmbedder>> text_embedder_or =
      TextEmbedder::CreateFromOptions(options);
  EXPECT_EQ(text_embedder_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(text_embedder_or.status().message(),
              ::testing::HasSubstr("Invalid embedding_options"));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  TextEmbedderOptions options;

  StatusOr<std::unique_ptr<TextEmbedder>> text_embedder_or =
      TextEmbedder::CreateFromOptions(options);

  EXPECT_EQ(text_embedder_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      text_embedder_or.status().GetPayload(support::kTfLiteSupportPayload),
      ::testing::Optional(absl::Cord(
          absl::StrCat(support::TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST(EmbedTest, SucceedsWithMobileBertModel) {
  TextEmbedderOptions options = GetBasicOptions(kMobileBert);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<TextEmbedder> text_embedder,
                       TextEmbedder::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto result0,
      text_embedder->Embed("it's a charming and often affecting journey"));
  EXPECT_EQ(result0.embeddings_size(), 1);
  EXPECT_EQ(result0.embeddings(0).feature_vector().value_float_size(), 512);

  EXPECT_NEAR(result0.embeddings(0).feature_vector().value_float(0), 19.9016f,
              kValueDiffTolerance);

  SUPPORT_ASSERT_OK_AND_ASSIGN(auto result1,
                       text_embedder->Embed("what a great and fantastic trip"));
  EXPECT_EQ(result1.embeddings_size(), 1);
  EXPECT_EQ(result1.embeddings(0).feature_vector().value_float_size(), 512);

  EXPECT_NEAR(result1.embeddings(0).feature_vector().value_float(0), 22.626251f,
              kValueDiffTolerance);

  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      double similarity,
      TextEmbedder::CosineSimilarity(result0.embeddings(0).feature_vector(),
                                     result1.embeddings(0).feature_vector()));
  double expected_similarity = 0.969514;
  EXPECT_NEAR(similarity, expected_similarity, kSimilarityTolerancy);
}

TEST(EmbedTest, SucceedsWithRegexModel) {
  TextEmbedderOptions options = GetBasicOptions(kRegexOneEmbeddingModel);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<TextEmbedder> text_embedder,
                       TextEmbedder::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto result0,
      text_embedder->Embed("it's a charming and often affecting journey"));
  EXPECT_EQ(result0.embeddings_size(), 1);
  EXPECT_EQ(result0.embeddings(0).feature_vector().value_float_size(), 16);

  EXPECT_NEAR(result0.embeddings(0).feature_vector().value_float(0), 0.0309356f,
              kValueDiffTolerance);

  SUPPORT_ASSERT_OK_AND_ASSIGN(auto result1,
                       text_embedder->Embed("what a great and fantastic trip"));
  EXPECT_EQ(result1.embeddings_size(), 1);
  EXPECT_EQ(result1.embeddings(0).feature_vector().value_float_size(), 16);

  EXPECT_NEAR(result1.embeddings(0).feature_vector().value_float(0), 0.0312863f,
              kValueDiffTolerance);

  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      double similarity,
      TextEmbedder::CosineSimilarity(result0.embeddings(0).feature_vector(),
                                     result1.embeddings(0).feature_vector()));
  double expected_similarity = 0.999937;
  EXPECT_NEAR(similarity, expected_similarity, kSimilarityTolerancy);
}

TEST(EmbedTest, SucceedsWithUniversalSentenceEncoder) {
  TextEmbedderOptions options = GetBasicOptions(kUniversalSentenceEndoder);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TextEmbedder> text_embedder,
      TextEmbedder::CreateFromOptions(options, CreateTextOpResolver()));

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto result0,
      text_embedder->Embed("it's a charming and often affecting journey"));
  EXPECT_EQ(result0.embeddings_size(), 1);
  EXPECT_EQ(result0.embeddings(0).feature_vector().value_float_size(), 100);

  EXPECT_NEAR(result0.embeddings(0).feature_vector().value_float(0), 1.422951f,
              kValueDiffTolerance);

  SUPPORT_ASSERT_OK_AND_ASSIGN(auto result1,
                       text_embedder->Embed("what a great and fantastic trip"));
  EXPECT_EQ(result1.embeddings_size(), 1);
  EXPECT_EQ(result1.embeddings(0).feature_vector().value_float_size(), 100);

  EXPECT_NEAR(result1.embeddings(0).feature_vector().value_float(0), 1.404664f,
              kValueDiffTolerance);

  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      double similarity,
      TextEmbedder::CosineSimilarity(result0.embeddings(0).feature_vector(),
                                     result1.embeddings(0).feature_vector()));
  double expected_similarity = 0.851961;
  EXPECT_NEAR(similarity, expected_similarity, kSimilarityTolerancy);
}

TEST(GetEmbeddingDimension, Succeeds) {
  // Create embedder.
  TextEmbedderOptions options = GetBasicOptions(kMobileBert);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<TextEmbedder> text_embedder,
                       TextEmbedder::CreateFromOptions(options));

  EXPECT_EQ(text_embedder->GetEmbeddingDimension(0), 512);
  EXPECT_EQ(text_embedder->GetEmbeddingDimension(1), -1);
  EXPECT_EQ(text_embedder->GetEmbeddingDimension(-1), -1);
}

TEST(GetNumberOfOutputLayers, Succeeds) {
  TextEmbedderOptions options = GetBasicOptions(kMobileBert);
  // No Embedding options means all head get a default option.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<TextEmbedder> text_embedder,
                       TextEmbedder::CreateFromOptions(options));
  EXPECT_EQ(text_embedder->GetNumberOfOutputLayers(), kNumberOfOutputLayers);
}

}  // namespace
}  // namespace text
}  // namespace task
}  // namespace tflite
