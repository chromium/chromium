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

#include "tensorflow_lite_support/cc/task/text/bert_clu_annotator.h"

#include <fcntl.h>

#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/text/proto/clu_proto_inc.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace text {
namespace clu {

namespace {

using ::testing::Optional;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::proto::TextFormat;
using ::tflite::task::ParseTextProtoOrDie;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "text/";

constexpr char kTestBertCluAnnotatorModelWithMetadataPath[] =
    "bert_clu_annotator_with_metadata.tflite";

std::string GetFullPath(absl::string_view file_name) {
  return JoinPath("./" /*test src dir*/, kTestDataDirectory,
                  file_name);
}

// Checks that the two provided `CluResponse` protos are equal.
// If the proto definition changes, please also change this function.
// TODO(b/235865164): Add checks for `domains` and `intents` in this util.
void ExpectApproximatelyEqual(const CluResponse& actual,
                              const CluResponse& expected) {
  // Maybe ignore, if domains_size == 0.
  if (expected.domains_size() > 0) {
    ASSERT_EQ(actual.domains_size(), expected.domains_size());
    for (int i = 0; i < actual.domains_size(); ++i) {
      const auto& a = actual.domains(i);
      const auto& b = expected.domains(i);
      EXPECT_THAT(a.display_name(), b.display_name());
    }
  }

  ASSERT_EQ(actual.categorical_slots_size(), expected.categorical_slots_size());
  for (int i = 0; i < actual.categorical_slots_size(); ++i) {
    const auto& a = actual.categorical_slots(i);
    const auto& b = expected.categorical_slots(i);
    EXPECT_EQ(a.slot(), b.slot());
    EXPECT_EQ(a.prediction().display_name(), b.prediction().display_name());
  }

  ASSERT_EQ(actual.mentioned_slots_size(),
            expected.mentioned_slots_size());
  for (int i = 0; i < actual.mentioned_slots_size(); ++i) {
    const auto& a = actual.mentioned_slots(i);
    const auto& b = expected.mentioned_slots(i);
    EXPECT_EQ(a.slot(), b.slot());
    EXPECT_EQ(a.mention().value(), b.mention().value());
    EXPECT_EQ(a.mention().start(), b.mention().start());
    EXPECT_EQ(a.mention().end(), b.mention().end());
  }
}

TEST(BertCluAnnotatorTest, CreateFromOptionsFailsWithMissingBaseOptions) {
  BertCluAnnotatorOptions options;
  auto clu_annotator = BertCluAnnotator::CreateFromOptions(options);

  ASSERT_EQ(clu_annotator.status().code(), absl::StatusCode::kInvalidArgument);
  ASSERT_EQ(clu_annotator.status().message(),
            "Missing mandatory `base_options` field");
  EXPECT_THAT(clu_annotator.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST(BertCluAnnotatorTest, TestAnnotatorEmptyRequest) {
  BertCluAnnotatorOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestBertCluAnnotatorModelWithMetadataPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<CluAnnotator> clu_annotator,
                       BertCluAnnotator::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto response,
                       clu_annotator->Annotate(CluRequest()));

  ExpectApproximatelyEqual(response, CluResponse());
}

TEST(BertCluAnnotatorTest, TestAnnotatorEmptyUtterance) {
  BertCluAnnotatorOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestBertCluAnnotatorModelWithMetadataPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<CluAnnotator> clu_annotator,
                       BertCluAnnotator::CreateFromOptions(options));
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            utterances: ""
                                          )pb",
                                          &request));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto response, clu_annotator->Annotate(request));
  ExpectApproximatelyEqual(response, CluResponse());
}

TEST(BertCluAnnotatorTest, TestAnnotatorBasic) {
  BertCluAnnotatorOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestBertCluAnnotatorModelWithMetadataPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<CluAnnotator> clu_annotator,
                       BertCluAnnotator::CreateFromOptions(options));
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        utterances: "I would like to make a restaurant reservation at morning 11:15?"
        utterances: "Which restaurant do you want to go to?"
        utterances: "Can I get a reservation for two people at Andes Cafe?"
      )pb",
      &request));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto response, clu_annotator->Annotate(request));

  ExpectApproximatelyEqual(
      response, ParseTextProtoOrDie<CluResponse>(R"pb(
        domains { display_name: "Restaurants" }
        categorical_slots {
          slot: "number_of_seats"
          prediction: { display_name: "2" }
        }
        mentioned_slots {
          slot: "restaurant_name"
          mention: { value: "Andes Cafe" start: 42 end: 52 }
        }
      )pb"));
}

TEST(BertCluAnnotatorTest, TestAnnotatorThresholds) {
  BertCluAnnotatorOptions options;
  options.set_domain_threshold(0.99);
  options.set_categorical_slot_threshold(0.99);
  options.set_mentioned_slot_threshold(0.99);
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestBertCluAnnotatorModelWithMetadataPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<CluAnnotator> clu_annotator,
                       BertCluAnnotator::CreateFromOptions(options));
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        utterances: "I would like to make a restaurant reservation at morning 11:15?"
        utterances: "Which restaurant do you want to go to?"
        utterances: "Can I get a reservation for two people at Andes Cafe?"
      )pb",
      &request));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto response, clu_annotator->Annotate(request));
  ExpectApproximatelyEqual(response,
                           ParseTextProtoOrDie<CluResponse>(R"pb()pb"));
}

}  // namespace
}  // namespace clu
}  // namespace text
}  // namespace task
}  // namespace tflite
