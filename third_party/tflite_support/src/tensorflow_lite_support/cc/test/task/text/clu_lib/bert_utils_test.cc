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

#include "tensorflow_lite_support/cc/task/text/clu_lib/bert_utils.h"

#include <memory>

#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"

namespace tflite::task::text::clu {
namespace {

class BertUtilsTestClass : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::vector<std::string> wordpiece_vocab({
        "[PAD]",    // 0
        "[UNK]",    // 1
        "[CLS]",    // 2
        "[SEP]",    // 3
        "?",        // 4
        "wanna",    // 5
        "see",      // 6
        "2",        // 7
        "to",       // 8
        "##night",  // 9
        ",",        // 10
        "good",     // 11
        "he",       // 12
        "##llo",    // 13
        "how",      // 14
        "are",      // 15
        "you",      // 16
        "plan",     // 17
        "##s",      // 18
        "gr",       // 19
        "##eat"     // 20
    });
    tokenizer_ =
        std::make_unique<tflite::support::text::tokenizer::BertTokenizer>(
            wordpiece_vocab);
  }

  std::unique_ptr<tflite::support::text::tokenizer::BertTokenizer> tokenizer_;
};

TEST_F(BertUtilsTestClass, ZeroHistoryNotTrucated) {
  const std::vector<absl::string_view> conversations_in_reverse_order = {
      "wanna see avenger 2 tonight?", "any plans tonight?",
      "good, how are you?", "hello how are you?"};
  const int max_seq_length = 10;
  const int max_history_turns = 0;

  const std::vector<int> expected_token_ids = {
      2,  // [CLS]
      5,  // "wanna"
      6,  // "see"
      1,  // "[UNK]"
      7,  // "0"
      8,  // "to"
      9,  // "##night"
      4,  // "?"
      3   // [SEP]
  };
  const std::vector<std::pair<int, int>> expected_token_alignments = {
      {-1, -1},  // [CLS]
      {0, 5},    // "wanna"
      {6, 9},    // "see"
      {10, 17},  // "[UNK]"
      {18, 19},  // "0"
      {20, 22},  // "to"
      {22, 27},  // "##night"
      {27, 28},  // "?"
      {28, 28}   // [SEP]
  };
  const std::vector<int> expected_first_subword_indicators = {
      0,  // [CLS]
      1,  // "wanna"
      1,  // "see"
      1,  // "[UNK]"
      1,  // "0"
      1,  // "to"
      0,  // "##night"
      1,  // "?"
      0   // [SEP]
  };
  const std::vector<int> expected_segment_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  // "to"
      0,  // "##night"
      0,  // "?"
      0   // [SEP]
  };
  const std::vector<int> expected_turn_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  // "to"
      0,  // "##night"
      0,  // "?"
      0   // [SEP]
  };

  std::vector<int> token_ids;
  std::vector<std::pair<int, int>> token_alignments;
  std::vector<int> subword_indicators;
  std::vector<int> segment_id_list;
  std::vector<int> turn_id_list;
  SUPPORT_ASSERT_OK(BertPreprocessing(tokenizer_.get(), conversations_in_reverse_order,
                              max_seq_length, max_history_turns, &token_ids,
                              &token_alignments, &subword_indicators,
                              &segment_id_list, &turn_id_list));
  EXPECT_THAT(token_ids, expected_token_ids);
  EXPECT_THAT(token_alignments, expected_token_alignments);
  EXPECT_THAT(subword_indicators, expected_first_subword_indicators);
  EXPECT_THAT(segment_id_list, expected_segment_id_list);
  EXPECT_THAT(turn_id_list, expected_turn_id_list);
}

TEST_F(BertUtilsTestClass, ZeroHistoryTrucated) {
  const std::vector<absl::string_view> conversations_in_reverse_order = {
      "wanna see avenger 2 tonight?", "any plans tonight?",
      "good, how are you?", "hello how are you?"};
  const int max_seq_length = 7;
  const int max_history_turns = 0;

  // Subword token "to" of "tonight" is also truncated based on whole words.
  const std::vector<int> expected_token_ids = {
      2,  // [CLS]
      5,  // "wanna"
      6,  // "see"
      1,  // "[UNK]"
      7,  // "0"
      3   // [SEP]
  };
  const std::vector<std::pair<int, int>> expected_token_alignments = {
      {-1, -1},  // [CLS]
      {0, 5},    // "wanna"
      {6, 9},    // "see"
      {10, 17},  // "[UNK]"
      {18, 19},  // "0"
      {19, 19}   // [SEP]
  };
  const std::vector<int> expected_first_subword_indicators = {
      0,  // [CLS]
      1,  // "wanna"
      1,  // "see"
      1,  // "[UNK]"
      1,  // "0"
      0   // [SEP]
  };
  const std::vector<int> expected_segment_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0   // [SEP]
  };
  const std::vector<int> expected_turn_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0   // [SEP]
  };

  std::vector<int> token_ids;
  std::vector<std::pair<int, int>> token_alignments;
  std::vector<int> subword_indicators;
  std::vector<int> segment_id_list;
  std::vector<int> turn_id_list;
  SUPPORT_ASSERT_OK(BertPreprocessing(tokenizer_.get(), conversations_in_reverse_order,
                              max_seq_length, max_history_turns, &token_ids,
                              &token_alignments, &subword_indicators,
                              &segment_id_list, &turn_id_list));
  EXPECT_THAT(token_ids, expected_token_ids);
  EXPECT_THAT(token_alignments, expected_token_alignments);
  EXPECT_THAT(subword_indicators, expected_first_subword_indicators);
  EXPECT_THAT(segment_id_list, expected_segment_id_list);
  EXPECT_THAT(turn_id_list, expected_turn_id_list);
}

TEST_F(BertUtilsTestClass, WithHistoryNotTrucated) {
  const std::vector<absl::string_view> conversations_in_reverse_order = {
      "wanna see avenger 2 tonight?", "any plans tonight?",
      "good, how are you?", "hello how are you?"};
  const int max_seq_length = 30;
  const int max_history_turns = 2;

  const std::vector<int> expected_token_ids = {
      2,   // [CLS]
      5,   // "wanna"
      6,   // "see"
      1,   // "[UNK]"
      7,   // "0"
      8,   //  "to"
      9,   //  "##night"
      4,   //  "?"
      3,   // [SEP]
      1,   // [UNK]
      17,  //  "plan"
      18,  //  "##s"
      8,   //  "to"
      9,   //  "##night"
      4,   //  "?"
      3,   // [SEP]
      11,  //  "good"
      10,  // ","
      14,  // "how"
      15,  // "are"
      16,  // "you"
      4,   // "?"
      3    // [SEP]
  };
  const std::vector<std::pair<int, int>> expected_token_alignments = {
      {-1, -1},  // [CLS]
      {0, 5},    // "wanna"
      {6, 9},    // "see"
      {10, 17},  // "[UNK]"
      {18, 19},  // "0"
      {20, 22},  //  "to"
      {22, 27},  //  "##night"
      {27, 28},  //  "?"
      {28, 28},  // [SEP]
      {0, 3},    // [UNK]
      {4, 8},    //  "plan"
      {8, 9},    //  "##s"
      {10, 12},  //  "to"
      {12, 17},  //  "##night"
      {17, 18},  //  "?"
      {18, 18},  // [SEP]
      {0, 4},    //  "good"
      {4, 5},    // ","
      {6, 9},    // "how"
      {10, 13},  // "are"
      {14, 17},  // "you"
      {17, 18},  // "?"
      {18, 18}   // [SEP]
  };
  const std::vector<int> expected_first_subword_indicators = {
      0,  // [CLS]
      1,  // "wanna"
      1,  // "see"
      1,  // "[UNK]"
      1,  // "0"
      1,  //  "to"
      0,  //  "##night"
      1,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      0,  //  "##s"
      1,  //  "to"
      0,  //  "##night"
      1,  //  "?"
      0,  // [SEP]
      1,  //  "good"
      1,  // ","
      1,  // "how"
      1,  // "are"
      1,  // "you"
      1,  // "?"
      0   // [SEP]
  };
  const std::vector<int> expected_segment_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  //  "to"
      0,  //  "##night"
      0,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      1,  //  "##s"
      1,  //  "to"
      1,  //  "##night"
      1,  //  "?"
      1,  // [SEP]
      1,  //  "good"
      1,  // ","
      1,  // "how"
      1,  // "are"
      1,  // "you"
      1,  // "?"
      1   // [SEP]
  };
  const std::vector<int> expected_turn_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  //  "to"
      0,  //  "##night"
      0,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      1,  //  "##s"
      1,  //  "to"
      1,  //  "##night"
      1,  //  "?"
      1,  // [SEP]
      2,  //  "good"
      2,  // ","
      2,  // "how"
      2,  // "are"
      2,  // "you"
      2,  // "?"
      2   // [SEP]
  };

  std::vector<int> token_ids;
  std::vector<std::pair<int, int>> token_alignments;
  std::vector<int> subword_indicators;
  std::vector<int> segment_id_list;
  std::vector<int> turn_id_list;
  SUPPORT_ASSERT_OK(BertPreprocessing(tokenizer_.get(), conversations_in_reverse_order,
                              max_seq_length, max_history_turns, &token_ids,
                              &token_alignments, &subword_indicators,
                              &segment_id_list, &turn_id_list));
  EXPECT_THAT(token_ids, expected_token_ids);
  EXPECT_THAT(token_alignments, expected_token_alignments);
  EXPECT_THAT(subword_indicators, expected_first_subword_indicators);
  EXPECT_THAT(segment_id_list, expected_segment_id_list);
  EXPECT_THAT(turn_id_list, expected_turn_id_list);
}

TEST_F(BertUtilsTestClass, WithHistoryTrucated) {
  const std::vector<absl::string_view> conversations_in_reverse_order = {
      "wanna see avenger 2 tonight?", "any plans tonight?",
      "great, how are you?", "hello how are you?"};
  const int max_seq_length = 18;
  const int max_history_turns = 3;

  // The utterance "Great, how are you?" and its [SEP] are all truncated because
  // the truncation is based on whole words.
  const std::vector<int> expected_token_ids = {
      2,   // [CLS]
      5,   // "wanna"
      6,   // "see"
      1,   // "[UNK]"
      7,   // "0"
      8,   //  "to"
      9,   //  "##night"
      4,   //  "?"
      3,   // [SEP]
      1,   // [UNK]
      17,  //  "plan"
      18,  //  "##s"
      8,   //  "to"
      9,   //  "##night"
      4,   //  "?"
      3,   // [SEP]
  };
  const std::vector<std::pair<int, int>> expected_token_alignments = {
      {-1, -1},  // [CLS]
      {0, 5},    // "wanna"
      {6, 9},    // "see"
      {10, 17},  // "[UNK]"
      {18, 19},  // "0"
      {20, 22},  //  "to"
      {22, 27},  //  "##night"
      {27, 28},  //  "?"
      {28, 28},  // [SEP]
      {0, 3},    // [UNK]
      {4, 8},    //  "plan"
      {8, 9},    //  "##s"
      {10, 12},  //  "to"
      {12, 17},  //  "##night"
      {17, 18},  //  "?"
      {18, 18}   // [SEP]
  };
  const std::vector<int> expected_first_subword_indicators = {
      0,  // [CLS]
      1,  // "wanna"
      1,  // "see"
      1,  // "[UNK]"
      1,  // "0"
      1,  //  "to"
      0,  //  "##night"
      1,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      0,  //  "##s"
      1,  //  "to"
      0,  //  "##night"
      1,  //  "?"
      0,  // [SEP]
  };
  const std::vector<int> expected_segment_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  //  "to"
      0,  //  "##night"
      0,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      1,  //  "##s"
      1,  //  "to"
      1,  //  "##night"
      1,  //  "?"
      1,  // [SEP]
  };
  const std::vector<int> expected_turn_id_list = {
      0,  // [CLS]
      0,  // "wanna"
      0,  // "see"
      0,  // "[UNK]"
      0,  // "0"
      0,  //  "to"
      0,  //  "##night"
      0,  //  "?"
      0,  // [SEP]
      1,  // [UNK]
      1,  //  "plan"
      1,  //  "##s"
      1,  //  "to"
      1,  //  "##night"
      1,  //  "?"
      1,  // [SEP]
  };

  std::vector<int> token_ids;
  std::vector<std::pair<int, int>> token_alignments;
  std::vector<int> subword_indicators;
  std::vector<int> segment_id_list;
  std::vector<int> turn_id_list;
  SUPPORT_ASSERT_OK(BertPreprocessing(tokenizer_.get(), conversations_in_reverse_order,
                              max_seq_length, max_history_turns, &token_ids,
                              &token_alignments, &subword_indicators,
                              &segment_id_list, &turn_id_list));
  EXPECT_THAT(token_ids, expected_token_ids);
  EXPECT_THAT(token_alignments, expected_token_alignments);
  EXPECT_THAT(subword_indicators, expected_first_subword_indicators);
  EXPECT_THAT(segment_id_list, expected_segment_id_list);
  EXPECT_THAT(turn_id_list, expected_turn_id_list);
}
}  // namespace

}  // namespace tflite::task::text::clu
