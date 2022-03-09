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

#include "tensorflow_lite_support/cc/task/text/clu_lib/slot_repr.h"

#include "absl/strings/str_join.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite::task::text::clu {
namespace {

TEST(SlotTagging, SlotRepr) {
  const auto slot_repr = SlotRepr::Create("num_people", "FLIGHT", false);
  EXPECT_EQ(slot_repr.BTag(), "B-FLIGHT~~num_people");
  EXPECT_EQ(slot_repr.ITag(), "I-FLIGHT~~num_people");
  EXPECT_EQ(slot_repr.Domain(), "FLIGHT");
  EXPECT_EQ(slot_repr.Name(), "num_people");
}

TEST(SlotTagging, SlotReprWithSharing) {
  const auto slot_repr = SlotRepr::Create("num_people", "FLIGHT", true);
  EXPECT_EQ(slot_repr.BTag(), "B-num_people");
  EXPECT_EQ(slot_repr.Domain(), "");
  EXPECT_EQ(slot_repr.Name(), "num_people");
}

TEST(SlotTagging, SlotRepr2) {
  const auto status_or_val = SlotRepr::CreateFromIob(
      "B-nlp_semantic_parsing.models.planning.Planning.planning~~anchor[*]."
      "time");
  SUPPORT_ASSERT_OK(status_or_val);
  const auto& slot_repr = status_or_val.value();
  EXPECT_EQ(slot_repr.Domain(),
            "nlp_semantic_parsing.models.planning.Planning.planning");
  EXPECT_EQ(slot_repr.Name(), "anchor[*].time");
}

TEST(SlotTagging, SlotRepr3) {
  const auto status_or_val = SlotRepr::CreateFromIob("B-num_people");
  SUPPORT_ASSERT_OK(status_or_val);
  const auto& slot_repr = status_or_val.value();
  EXPECT_EQ(slot_repr.FullName(), "num_people");
}

auto AssertSlotMentionStructEq(const SlotMentionStruct& lhs,
                               const SlotMentionStruct& rhs) {
  EXPECT_EQ(lhs.repr, rhs.repr);
  EXPECT_EQ(lhs.start, rhs.start);
  EXPECT_EQ(lhs.exclusive_end, rhs.exclusive_end);
  EXPECT_FLOAT_EQ(lhs.confidence, rhs.confidence);
}

TEST(SlotTagging, DecodeSlotChunks) {
  const std::vector<std::pair<int, int>> token_alignments = {
      {-1, -1}, {0, 3},   {4, 9},   {10, 14}, {15, 19},
      {20, 26}, {26, 27}, {27, 28}, {29, 29}};
  const absl::string_view tag_names[] = {"O",
                                         "O",
                                         "B-restaurant_name",
                                         "I-restaurant_name",
                                         "I-restaurant_name",
                                         "O",
                                         "B-date",
                                         "O",
                                         "O"};
  const float tag_probs[] = {0.9, 0.9, 0.8, 0.91, 0.82, 0.84, 0.9, 0.9, 0.9};
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto slots, DecodeSlotChunks(tag_names, tag_probs, token_alignments));
  EXPECT_EQ(2, slots.size());
  auto slot1 =
      SlotMentionStruct{SlotRepr::Create("restaurant_name"), 4, 19, 0.8};
  AssertSlotMentionStructEq(slot1, slots[0]);
  auto slot2 = SlotMentionStruct{SlotRepr::Create("date"), 26, 27, 0.9};
  AssertSlotMentionStructEq(slot2, slots[1]);
}

TEST(SlotTagging, DecodeSlotChunksEmptyToken) {
  const std::vector<std::pair<int, int>> token_alignments = {
      {-1, -1}, {0, 4}, {5, 5}, {5, 6}, {7, 7}};
  const absl::string_view tag_names[] = {"O", "B-time", "O", "O", "O"};
  const float tag_probs[] = {1.0, 0.9, 0.9, 0.91, 1.0};
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto slots, DecodeSlotChunks(tag_names, tag_probs, token_alignments));
  auto slot = SlotMentionStruct{SlotRepr::Create("time"), 0, 4, 0.9};
  AssertSlotMentionStructEq(slot, slots[0]);
}

TEST(SlotTagging, DecodeSlotChunksBTagAfterEos) {
  const std::vector<std::pair<int, int>> token_alignments = {
      {-1, -1}, {0, 3}, {5, 9}, {15, 15}};
  const absl::string_view tag_names[] = {"O", "B-X", "I-X", "O", "B-Y"};
  const float tag_probs[] = {1, 1, 1, 1, 1};
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      auto slots, DecodeSlotChunks(tag_names, tag_probs, token_alignments));
  EXPECT_EQ(1, slots.size());
  auto slot1 = SlotMentionStruct{SlotRepr::Create("X"), 0, 9, 1};
  AssertSlotMentionStructEq(slot1, slots[0]);
}

TEST(ResolveInconsistentIobTagSeq, Consistent) {
  std::vector<std::string> input_seq = {"O", "B-time", "I-time", "B-per", "O"};
  std::vector<std::string> output_seq = input_seq;
  SUPPORT_ASSERT_OK(ResolveInconsistentIobTagSeq(&output_seq));
  EXPECT_EQ(output_seq, input_seq);
}

TEST(ResolveInconsistentIobTagSeq, InConsistent) {
  std::vector<std::string> input_seq = {"O", "B-time", "I-per", "O"};
  SUPPORT_ASSERT_OK(ResolveInconsistentIobTagSeq(&input_seq));
  EXPECT_EQ(input_seq, std::vector<std::string>({"O", "B-time", "B-per", "O"}));

  input_seq = {"O", "O", "I-per", "O"};
  SUPPORT_ASSERT_OK(ResolveInconsistentIobTagSeq(&input_seq));
  EXPECT_EQ(input_seq, std::vector<std::string>({"O", "O", "B-per", "O"}));

  input_seq = {"O", "B-time", "I-time", "I-per", "O"};
  SUPPORT_ASSERT_OK(ResolveInconsistentIobTagSeq(&input_seq));
  EXPECT_EQ(input_seq,
            std::vector<std::string>({"O", "B-time", "I-time", "B-per", "O"}));

  input_seq = {"O",     "B-time", "I-time", "O",     "I-per",
               "B-per", "O",      "I-loc",  "I-loc", "O"};
  SUPPORT_ASSERT_OK(ResolveInconsistentIobTagSeq(&input_seq));
  EXPECT_EQ(input_seq,
            std::vector<std::string>({"O", "B-time", "I-time", "O", "B-per",
                                      "B-per", "O", "B-loc", "I-loc", "O"}));
}

}  // namespace
}  // namespace tflite::task::text::clu
