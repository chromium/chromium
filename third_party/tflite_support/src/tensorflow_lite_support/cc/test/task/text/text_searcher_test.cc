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

#include "tensorflow_lite_support/cc/task/text/text_searcher.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/text/proto/text_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
namespace tflite {
namespace task {
namespace text {
namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::processor::NearestNeighbor;
using ::tflite::task::processor::SearchResult;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/text/";
constexpr char kMobileBertEmbedder[] =
    "mobilebert_embedding_with_metadata.tflite";
constexpr char kMobileBertIndex[] = "mobilebert_index.ldb";
constexpr char kMobileBertSearcher[] = "mobilebert_searcher.tflite";
constexpr char kRegexEmbedder[] = "regex_one_embedding_with_metadata.tflite";
constexpr char kRegexIndex[] = "regex_index.ldb";
constexpr char kRegexSearcher[] = "regex_searcher.tflite";
constexpr char kUSEEmbedder[] =
    "universal_sentence_encoder_qa_with_metadata.tflite";
constexpr char kUSEIndex[] = "universal_sentence_encoder_index.ldb";
constexpr char kUSESearcher[] = "universal_sentence_encoder_searcher.tflite";

// Checks that the two provided `SearchResult`  protos are equal, with a
// tolerancy on floating-point scores to account for numerical instabilities.
void ExpectApproximatelyEqual(const SearchResult& actual,
                              const SearchResult& expected) {
  const float kPrecision = 1e-5;
  EXPECT_EQ(actual.nearest_neighbors_size(), expected.nearest_neighbors_size());
  for (int i = 0; i < actual.nearest_neighbors_size(); ++i) {
    const NearestNeighbor& a = actual.nearest_neighbors(i);
    const NearestNeighbor& b = expected.nearest_neighbors(i);
    EXPECT_EQ(a.metadata(), b.metadata());
    EXPECT_NEAR(a.distance(), b.distance(), kPrecision);
  }
}

std::unique_ptr<tflite::OpResolver> GetOpResolver(
    bool is_universal_sentence_encoder) {
  if (is_universal_sentence_encoder) {
    return CreateTextOpResolver();
  } else {
    return absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>();
  }
}

struct CreateFromOptionsParams {
  std::string name;
  std::string embedder_model_name;
  std::string searcher_model_name;
  bool is_universal_sentence_encoder;
  std::string index_name;
};

class CreateFromOptionsTest : public TestWithParam<CreateFromOptionsParams> {};

TEST_P(CreateFromOptionsTest, SucceedsWithStandaloneIndex) {
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));

  SUPPORT_ASSERT_OK(TextSearcher::CreateFromOptions(
      options, GetOpResolver(GetParam().is_universal_sentence_encoder)));
}

TEST_P(CreateFromOptionsTest, SucceedsWithMetadataIndex) {
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().searcher_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);

  SUPPORT_ASSERT_OK(TextSearcher::CreateFromOptions(
      options, GetOpResolver(GetParam().is_universal_sentence_encoder)));
}

TEST_P(CreateFromOptionsTest, FailsWithMissingModel) {
  TextSearcherOptions options;
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));

  StatusOr<std::unique_ptr<TextSearcher>> image_searcher_or =
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder));

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      image_searcher_or.status().message(),
      HasSubstr("Missing mandatory `model_file` field in `base_options`"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_P(CreateFromOptionsTest, FailsWithMissingIndex) {
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);

  StatusOr<std::unique_ptr<TextSearcher>> image_searcher_or =
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder));

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      image_searcher_or.status().message(),
      HasSubstr("Unable to find index file: SearchOptions.index_file is not "
                "set and no AssociatedFile with type SCANN_INDEX_FILE could be "
                "found in the output tensor metadata."));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(absl::StrCat(
                  TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError))));
}

TEST_P(CreateFromOptionsTest, FailsWithQuantization) {
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_embedding_options()->set_quantize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));

  StatusOr<std::unique_ptr<TextSearcher>> image_searcher_or =
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder));

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_searcher_or.status().message(),
              HasSubstr("Setting EmbeddingOptions.quantize = true is not "
                        "allowed in searchers"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_P(CreateFromOptionsTest, FailsWithInvalidMaxResults) {
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));
  options.mutable_search_options()->set_max_results(-1);

  StatusOr<std::unique_ptr<TextSearcher>> image_searcher_or =
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder));

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_searcher_or.status().message(),
              HasSubstr("SearchOptions.max_results must be > 0, found -1"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

INSTANTIATE_TEST_SUITE_P(
    CreateFromOptionsTest, CreateFromOptionsTest,
    Values(CreateFromOptionsParams{
               /* name= */ "Bert",
               /* embedder_model_name= */ kMobileBertEmbedder,
               /* searcher_model_name= */ kMobileBertSearcher,
               /* is_universal_sentence_encoder= */ false,
               /* index_name= */ kMobileBertIndex},
           CreateFromOptionsParams{/* name= */ "Regex",
                                   /* embedder_model_name= */ kRegexEmbedder,
                                   /* searcher_model_name= */ kRegexSearcher,
                                   /* is_universal_sentence_encoder= */ false,
                                   /* index_name= */ kRegexIndex},
           CreateFromOptionsParams{/* name= */ "USE",
                                   /* embedder_model_name= */ kUSEEmbedder,
                                   /* searcher_model_name= */ kUSESearcher,
                                   /* is_universal_sentence_encoder= */ true,
                                   /* index_name= */ kUSEIndex}),
    [](const TestParamInfo<CreateFromOptionsTest::ParamType>& info) {
      return info.param.name;
    });

struct SearchParams {
  std::string name;
  std::string embedder_model_name;
  std::string searcher_model_name;
  bool is_universal_sentence_encoder;
  std::string index_name;
  std::string expected_result;
};

class SearchTest : public TestWithParam<SearchParams> {};

TEST_P(SearchTest, SucceedsWithStandaloneIndex) {
  // Create Searcher.
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TextSearcher> searcher,
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder)));

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search("The weather was excellent."));

  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(GetParam().expected_result));
}

TEST_P(SearchTest, SucceedsWithMetadataIndex) {
  // Create Searcher.
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().searcher_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TextSearcher> searcher,
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder)));

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search("The weather was excellent."));

  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(GetParam().expected_result));
}

TEST_P(SearchTest, SucceedsWithMaxResults) {
  // Create Searcher.
  TextSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().embedder_model_name));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               GetParam().index_name));
  options.mutable_search_options()->set_max_results(2);
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TextSearcher> searcher,
      TextSearcher::CreateFromOptions(
          options, GetOpResolver(GetParam().is_universal_sentence_encoder)));

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search("The weather was excellent."));

  // Check results.
  SearchResult all_results =
      ParseTextProtoOrDie<SearchResult>(GetParam().expected_result);
  SearchResult expected_result;
  expected_result.add_nearest_neighbors()->CopyFrom(
      all_results.nearest_neighbors(0));
  expected_result.add_nearest_neighbors()->CopyFrom(
      all_results.nearest_neighbors(1));
  ExpectApproximatelyEqual(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    SearchTest, SearchTest,
    Values(SearchParams{
               /* name= */ "Bert",
               /* embedder_model_name= */ kMobileBertEmbedder,
               /* searcher_model_name= */ kMobileBertSearcher,
               /* is_universal_sentence_encoder= */ false,
               /* index_name= */ kMobileBertIndex,
               /* expected_result= */ R"pb(
                 nearest_neighbors {
                   metadata: "The weather was excellent."
                   distance: 0.0
                 }
                 nearest_neighbors {
                   metadata: "It was a sunny day."
                   distance: 0.11537
                 }
                 nearest_neighbors {
                   metadata: "The sun was shining on that day."
                   distance: 0.23002
                 }
                 nearest_neighbors {
                   metadata: "He was very happy with his newly bought car."
                   distance: 0.32456
                 }
                 nearest_neighbors {
                   metadata: "The cat is chasing after the mouse."
                   distance: 0.96693
                 }
               )pb"},
           SearchParams{
               /* name= */ "Regex",
               /* embedder_model_name= */ kRegexEmbedder,
               /* searcher_model_name= */ kRegexSearcher,
               /* is_universal_sentence_encoder= */ false,
               /* index_name= */ kRegexIndex,
               /* expected_result= */ R"pb(
                 nearest_neighbors {
                   metadata: "The weather was excellent."
                   distance: 0.0
                 }
                 nearest_neighbors {
                   metadata: "The sun was shining on that day."
                   distance: 0.00006
                 }
                 nearest_neighbors {
                   metadata: "The cat is chasing after the mouse."
                   distance: 0.00009
                 }
                 nearest_neighbors {
                   metadata: "It was a sunny day."
                   distance: 0.00011
                 }
                 nearest_neighbors {
                   metadata: "He was very happy with his newly bought car."
                   distance: 0.00012
                 }
               )pb"},
           SearchParams{
               /* name= */ "USE",
               /* embedder_model_name= */ kUSEEmbedder,
               /* searcher_model_name= */ kUSESearcher,
               /* is_universal_sentence_encoder= */ true,
               /* index_name= */ kUSEIndex,
               /* expected_result= */ R"pb(
                 nearest_neighbors {
                   metadata: "The weather was excellent."
                   distance: 0.0
                 }
                 nearest_neighbors {
                   metadata: "It was a sunny day."
                   distance: 0.14636
                 }
                 nearest_neighbors {
                   metadata: "The sun was shining on that day."
                   distance: 0.15222
                 }
                 nearest_neighbors {
                   metadata: "The cat is chasing after the mouse."
                   distance: 0.35997
                 }
                 nearest_neighbors {
                   metadata: "He was very happy with his newly bought car."
                   distance: 0.36693
                 }
               )pb"}),
    [](const TestParamInfo<SearchTest::ParamType>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace text
}  // namespace task
}  // namespace tflite
