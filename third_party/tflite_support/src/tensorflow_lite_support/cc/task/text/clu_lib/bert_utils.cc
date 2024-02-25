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

#include <string>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/ascii.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/constants.h"

namespace tflite::task::text::clu {

namespace {
// Segment id is an alias of `token_type_id` in BERT encoder. `0` corresponds to
// the first sentence in BERT input (i.e., `token_type_id = 0`).
constexpr int kSegmentIdForCurrentUtterance = 0;
// Segment id is an alias of `token_type_id` in BERT encoder. `1` corresponds to
// the first sentence in BERT input (i.e., `token_type_id = 1`).
constexpr int kSegmentIdForPreviousUtterances = 1;

// `1` indicates that the subword token is the first token of a word.
constexpr int kIsFirstTokenInAWord = 1;
// `0` indicates that the subword token is *not* the first token of a word.
constexpr int kNotFirstTokenInAWord = 0;

// The turn id of the current utterance. Because the input turns are in the
// reverse choronological order, the turn id of the current utterance is 0.
constexpr int kTurnIdForCurrentUtterance = 0;
}  // namespace

absl::Status BertPreprocessing(
    const tflite::support::text::tokenizer::BertTokenizer* tokenizer,
    const std::vector<absl::string_view>& utterances_in_reverse_order,
    int max_seq_length, int max_history_turns, std::vector<int>* out_token_ids,
    std::vector<std::pair<int, int>>* out_token_alignments,
    std::vector<int>* out_token_first_subword_indicators,
    std::vector<int>* out_segment_id_list, std::vector<int>* out_turn_id_list) {
  int cls_id;
  if (!tokenizer->LookupId(kClsToken, &cls_id)) {
    return absl::InternalError(
        absl::StrCat("Cannot locate id for ", kClsToken));
  }
  int sep_id;
  if (!tokenizer->LookupId(kSepToken, &sep_id)) {
    return absl::InternalError(
        absl::StrCat("Cannot locate id for ", kSepToken));
  }
  // Add the [CLS] sentinel token.
  out_token_ids->emplace_back(cls_id);
  out_token_alignments->emplace_back(
      -1, -1);  // No input tokens are aligned to [CLS].
  out_token_first_subword_indicators->push_back(kNotFirstTokenInAWord);
  out_segment_id_list->push_back(kSegmentIdForCurrentUtterance);
  out_turn_id_list->push_back(kTurnIdForCurrentUtterance);
  // For each turn, preprocess and truncate.
  const int max_turn_id =
      std::min(static_cast<int>(utterances_in_reverse_order.size()),
               max_history_turns + 1);
  for (int turn_id = 0; turn_id < max_turn_id; ++turn_id) {
    // Tokenize each turn.
    const auto& utterance = utterances_in_reverse_order[turn_id];
    std::string utterance_text = std::string(utterance);
    if (kUseLowerCase) {
      absl::AsciiStrToLower(&utterance_text);
    }
    support::text::tokenizer::WordpieceTokenizerResult tokenize_result =
        tokenizer->TokenizeWordpiece(utterance_text);
    if (tokenize_result.subwords.size() !=
        tokenize_result.wp_begin_offset.size()) {
      // Should never happen.
      return absl::InternalError(
          absl::StrCat("In BertTokenize result, subwords and begin_offset "
                       "should have the same size, found ",
                       tokenize_result.subwords.size(), " v.s. ",
                       tokenize_result.wp_begin_offset.size()));
    }
    if (tokenize_result.wp_begin_offset.size() !=
        tokenize_result.wp_end_offset.size()) {
      // Should never happen.
      return absl::InternalError(
          absl::StrCat("In BertTokenize result, begin_offset and end_offset"
                       "should have the same size, found ",
                       tokenize_result.wp_begin_offset.size(), " v.s. ",
                       tokenize_result.wp_end_offset.size()));
    }
    std::vector<int> turn_token_ids;
    turn_token_ids.reserve(tokenize_result.subwords.size());
    for (const auto& subword : tokenize_result.subwords) {
      int subword_id;
      if (!tokenizer->LookupId(subword, &subword_id)) {
        // Should never happen.
        return absl::InternalError(
            absl::StrCat("Cannot locate id for ", subword));
      }
      turn_token_ids.push_back(subword_id);
    }
    std::vector<std::pair<int, int>> turn_token_alignments;
    turn_token_alignments.reserve(tokenize_result.wp_begin_offset.size());
    for (int i = 0; i < tokenize_result.wp_begin_offset.size(); ++i) {
      turn_token_alignments.emplace_back(tokenize_result.wp_begin_offset[i],
                                         tokenize_result.wp_end_offset[i]);
    }
    std::vector<int> first_subword_indicators(tokenize_result.subwords.size(),
                                              kNotFirstTokenInAWord);
    int first_subword_pos = 0;
    for (int i = 0; i < tokenize_result.row_lengths.size(); ++i) {
      if (first_subword_pos < first_subword_indicators.size()) {
        first_subword_indicators[first_subword_pos] = kIsFirstTokenInAWord;
      }
      first_subword_pos += tokenize_result.row_lengths[i];
    }

    // Truncates if necessary.
    const int max_turn_len_before_sentinels =
        max_seq_length - 1 -
        out_token_ids->size();  // Reserve one position for the [SEP] sentinel.
    if (max_turn_len_before_sentinels <= 0) {
      // The entire turn needs to be truncated. And max_seq_length has been
      // reached.
      break;
    }
    if (turn_token_ids.size() > max_turn_len_before_sentinels) {
      // Truncate the three sequences based on whole word units. We want to make
      // sure that if a subword token is truncated, all the subword tokens of
      // that whole word is truncated.
      int truncating_position = max_turn_len_before_sentinels;
      while (truncating_position >= 0 &&
             first_subword_indicators[truncating_position] ==
                 kNotFirstTokenInAWord) {
        --truncating_position;
      }
      if (truncating_position <= 0) {
        // The entire turn needs to be truncated; it does not make sense to
        // append [SEP] in this case. And max_seq_length has been reached.
        break;
      }
      // 'truncating_position' now points to the position of the first natural
      // word to be truncated.
      turn_token_ids.resize(truncating_position);
      turn_token_alignments.resize(truncating_position);
      first_subword_indicators.resize(truncating_position);
    }

    // Append the current turn to the concatenated output.
    out_token_ids->insert(out_token_ids->end(), turn_token_ids.begin(),
                          turn_token_ids.end());
    out_token_alignments->insert(out_token_alignments->end(),
                                 turn_token_alignments.begin(),
                                 turn_token_alignments.end());
    out_token_first_subword_indicators->insert(
        out_token_first_subword_indicators->end(),
        first_subword_indicators.begin(), first_subword_indicators.end());
    const int segment_id = (turn_id == kTurnIdForCurrentUtterance
                                ? kSegmentIdForCurrentUtterance
                                : kSegmentIdForPreviousUtterances);
    out_segment_id_list->resize(
        out_segment_id_list->size() + turn_token_ids.size(), segment_id);
    out_turn_id_list->resize(out_turn_id_list->size() + turn_token_ids.size(),
                             turn_id);

    // Add the [SEP] sentinel token.
    const int last_token_end_pos =
        turn_token_alignments.empty() ? 0 : turn_token_alignments.back().second;
    out_token_ids->push_back(sep_id);
    out_token_alignments->emplace_back(last_token_end_pos, last_token_end_pos);
    out_token_first_subword_indicators->push_back(kNotFirstTokenInAWord);
    out_segment_id_list->push_back(segment_id);
    out_turn_id_list->push_back(turn_id);

    // Break if reaching max_seq_length.
    if (out_token_ids->size() >= max_seq_length) break;
  }
  if (out_token_ids->size() != out_token_alignments->size()) {
    return absl::InternalError(absl::StrCat(
        "The size of out_token_ids and out_token_alignments are not equal in "
        "BERT preprocessing.",
        out_token_ids->size(), " v.s. ", out_token_alignments->size()));
  }
  if (out_token_ids->size() != out_token_first_subword_indicators->size()) {
    return absl::InternalError(absl::StrCat(
        "The size of out_token_ids and out_token_first_subword_indicators are "
        "not equal in BERT preprocessing.",
        out_token_ids->size(), " v.s. ",
        out_token_first_subword_indicators->size()));
  }
  if (out_token_ids->size() != out_segment_id_list->size()) {
    return absl::InternalError(absl::StrCat(
        "The size of out_token_ids and out_segment_id_list are not equal in "
        "BERT preprocessing.",
        out_token_ids->size(), " v.s. ", out_segment_id_list->size()));
  }
  if (out_token_ids->size() != out_turn_id_list->size()) {
    return absl::InternalError(absl::StrCat(
        "The size of out_token_ids and out_turn_id_list are not equal in "
        "BERT preprocessing.",
        out_token_ids->size(), " v.s. ", out_turn_id_list->size()));
  }

  return absl::OkStatus();
}
}  // namespace tflite::task::text::clu
