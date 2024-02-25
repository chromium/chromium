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

#include "tensorflow_lite_support/cc/task/text/clu_lib/slot_tagging_output.h"

#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/text/proto/clu_proto_inc.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite::task::text::clu {
namespace {

using ::tflite::support::EqualsProto;
using ::tflite::task::ParseTextProtoOrDie;

// Checks that the two provided `CluResponse` protos are equal.
// If the proto definition changes, please also change this function.
void ExpectApproximatelyEqual(const CluResponse& actual,
                              const CluResponse& expected) {
  ASSERT_EQ(actual.mentioned_slots_size(),
            expected.mentioned_slots_size());
  for (int i = 0; i < actual.mentioned_slots_size(); ++i) {
    const auto& a = actual.mentioned_slots(i);
    const auto& b = expected.mentioned_slots(i);
    EXPECT_THAT(a, EqualsProto(b));
  }
}

TEST(SlotTaggingOutputTest, SlotModulePopulateResponseBasic) {
  // Assuming 6 tokens: _BOS_ book at 4 pm _EOS_
  std::vector<absl::string_view> tag_names{"O",      "O",      "O",
                                           "B-time", "I-time", "O"};
  std::vector<float> tag_scores{1.0, 0.9, 0.8, 0.7, 0.9, 1.0};
  std::vector<std::pair<int, int>> token_alignments{
      {-1, -1}, {0, 4}, {5, 7}, {8, 9}, {10, 12}, {12, 12}};
  std::vector<int> turn_ids{0, 0, 0, 0, 0, 0};
  std::vector<int> first_subword_indicators{0, 1, 1, 1, 1, 0};
  std::vector<absl::string_view> reverse_utterance_list_to_encode{
      "book at 4 pm"};
  // Domain predictions need to be pre-populated.
  CluResponse response;
  SUPPORT_ASSERT_OK(SlotModulePopulateResponse(
      tag_names, tag_scores.data(), token_alignments, turn_ids,
      first_subword_indicators, /*threshold=*/0.5,
      reverse_utterance_list_to_encode, &response));
  ExpectApproximatelyEqual(
      response, ParseTextProtoOrDie<CluResponse>(R"pb(
        mentioned_slots {
          slot: "time"
          mention { value: "4 pm" start: 8 end: 12 score: 0.7 }
        }
      )pb"));
}

TEST(SlotTaggingOutputTest, SlotModulePopulateResponseBelowThreshold) {
  // Assuming 6 tokens: _BOS_ book at 4 pm _EOS_
  std::vector<absl::string_view> tag_names{"O",      "O",      "O",
                                           "B-time", "I-time", "O"};
  std::vector<float> tag_scores{1.0, 0.9, 0.8, 0.7, 0.9, 1.0};
  std::vector<std::pair<int, int>> token_alignments{
      {-1, -1}, {0, 4}, {5, 7}, {8, 9}, {10, 12}, {12, 12}};
  std::vector<int> turn_ids{0, 0, 0, 0, 0, 0};
  std::vector<int> first_subword_indicators{0, 1, 1, 1, 1, 0};
  std::vector<absl::string_view> reverse_utterance_list_to_encode{
      "book at 4 pm"};
  // Domain predictions need to be pre-populated.
  CluResponse response;
  SUPPORT_ASSERT_OK(SlotModulePopulateResponse(
      tag_names, tag_scores.data(), token_alignments, turn_ids,
      first_subword_indicators, /*threshold=*/0.8,
      reverse_utterance_list_to_encode, &response));
  ExpectApproximatelyEqual(response,
                           ParseTextProtoOrDie<CluResponse>(R"pb()pb"));
}

TEST(SlotTaggingOutputTest, SlotModulePopulateResponseTruncated) {
  // Assuming 6 tokens: _BOS_ book at 4 pm _EOS_
  // The last token "pm" is got truncated. _EOS_ is added after truncation.
  std::vector<absl::string_view> tag_names{"O", "O", "O", "B-time", "O"};
  std::vector<float> tag_scores{1.0, 0.9, 0.8, 0.6, 0.9};
  std::vector<std::pair<int, int>> token_alignments{
      {-1, -1}, {0, 4}, {5, 7}, {8, 9}, {10, 12}, {12, 12}};
  std::vector<int> turn_ids{0, 0, 0, 0, 0, 0};
  std::vector<int> first_subword_indicators{0, 1, 1, 1, 1, 0};
  std::vector<absl::string_view> reverse_utterance_list_to_encode{
      "book at 4 pm"};
  // Domain predictions need to be pre-populated.
  CluResponse response;
  SUPPORT_ASSERT_OK(SlotModulePopulateResponse(
      tag_names, tag_scores.data(), token_alignments, turn_ids,
      first_subword_indicators, /*threshold=*/0.5,
      reverse_utterance_list_to_encode, &response));
  ExpectApproximatelyEqual(
      response, ParseTextProtoOrDie<CluResponse>(R"pb(
        mentioned_slots {
          slot: "time"
          mention: { value: "4" start: 8 end: 9 score: 0.6 }
        }
      )pb"));
}

TEST(SlotTaggingOutputTest,
     SlotModulePopulateResponseMultipleTurnsPaddedPredictingOnFirstSubWord) {
  // Assume that the current turn with two history turns (in the reverse
  // chronological order):
  //  current utterance: "[CLS] book a ##t 4 pm [SEP]"
  //  the last turn: "Sun ##day ? [SEP]"
  //  the turn before the last: "book for 2 peo ##ple [SEP]"
  //
  // Note that "at", "Sunday", and "people" are broken into multiple subwords.
  // Also assume that max_seq_length of BERT is 18 (with one padding).
  std::vector<absl::string_view> reverse_utterance_list_to_encode{
      "book at 4 pm",
      "Sunday?",
      "book for 2 people",
  };
  std::vector<absl::string_view> tag_names{
      "O",         // [CLS]
      "O",         // "book"
      "O",         // "a"
      "B-dummy",   // "##t"
      "B-time",    // "4"
      "I-time",    // "pm"
      "O",         // [SEP]
      "B-date",    // "Sun"
      "O",         // "##day"
      "O",         // "?"
      "O",         // [SEP]
      "O",         // "book"
      "O",         // "for"
      "B-people",  // "2"
      "I-people",  // "peo"
      "O",         // "##ple"
      "O",         // [SEP]
      "O",         // [PAD]
  };
  std::vector<float> tag_scores{
      1.0,  // [CLS]
      0.9,  // "book"
      0.8,  // "a"
      0.9,  // "##t"
      0.7,  // "4"
      0.9,  // "pm"
      1.0,  // [SEP]
      0.8,  // "Sun"
      0.9,  // "##day"
      0.9,  // "?"
      0.7,  // [SEP]
      1.0,  // "book"
      0.9,  // "for"
      0.8,  // "2"
      0.5,  // "peo"
      0.9,  // "##ple"
      0.8,  // [SEP]
      1.0   // [PAD]
  };
  std::vector<std::pair<int, int>> token_alignments{
      {-1, -1},  // [CLS]
      {0, 4},    // "book"
      {5, 6},    // "a"
      {6, 7},    // "##t"
      {8, 9},    // "4"
      {10, 12},  // "pm"
      {12, 12},  // [SEP]
      {0, 3},    // "Sun"
      {3, 6},    // "##day"
      {7, 8},    // "?"
      {8, 8},    // [SEP]
      {0, 4},    // "book"
      {5, 8},    // "for"
      {9, 11},   // "2"
      {12, 15},  // "peo"
      {15, 18},  // "##ple"
      {18, 18}   // [SEP]
  };
  std::vector<int> token_turn_ids{
      0,  // [CLS]
      0,  // "book"
      0,  // "a"
      0,  // "##t"
      0,  // "4"
      0,  // "pm"
      0,  // [SEP]
      1,  // "Sun"
      1,  // "##day"
      1,  // "?"
      1,  // [SEP]
      2,  // "book"
      2,  // "for"
      2,  // "2"
      2,  // "peo"
      2,  // "##ple"
      2,  // [SEP]
  };
  std::vector<int> first_subword_indicators{
      0,  // [CLS]
      1,  // "book"
      1,  // "a"
      0,  // "##t"
      1,  // "4"
      1,  // "pm"
      0,  // [SEP]
      1,  // "Sun"
      0,  // "##day"
      1,  // "?"
      0,  // [SEP]
      1,  // "book"
      1,  // "for"
      1,  // "2"
      1,  // "peo"
      0,  // "##ple"
      0,  // [SEP]
  };
  // Domain predictions need to be pre-populated.
  CluResponse response;
  SUPPORT_ASSERT_OK(SlotModulePopulateResponse(
      tag_names, tag_scores.data(), token_alignments, token_turn_ids,
      first_subword_indicators, /*threshold=*/0.5,
      reverse_utterance_list_to_encode, &response));
  ExpectApproximatelyEqual(
      response, ParseTextProtoOrDie<CluResponse>(R"pb(
        mentioned_slots {
          slot: "time"
          mention: { value: "4 pm" start: 8 end: 12 score: 0.7 }
        }
      )pb"));
}
}  // namespace
}  // namespace tflite::task::text::clu
