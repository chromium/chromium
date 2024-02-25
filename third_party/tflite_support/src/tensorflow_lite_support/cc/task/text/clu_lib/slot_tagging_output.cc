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

#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "absl/types/span.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/slot_repr.h"

namespace tflite::task::text::clu {
namespace {

absl::StatusOr<std::vector<SlotMentionStruct>>
DecodeSlotChunksPredictOnFirstSubword(
    int cur_turn_start, int cur_turn_end, int seq_len,
    const absl::Span<const absl::string_view> tags_as_span,
    const absl::Span<const float> confidences_as_span,
    const absl::Span<const std::pair<int, int>> token_alignments_as_span,
    const std::vector<int>& first_subword_indicators) {
  // Construct the vectors by select only first subword tokens and extend
  // the token alignments to the whole word.
  std::vector<absl::string_view> first_subword_tag_names;
  first_subword_tag_names.reserve(seq_len);
  std::vector<float> first_subword_tag_probs;
  first_subword_tag_probs.reserve(seq_len);
  std::vector<std::pair<int, int>> whole_word_token_alignments;
  whole_word_token_alignments.reserve(seq_len);
  for (int idx = cur_turn_start; idx < cur_turn_end; ++idx) {
    if (first_subword_indicators[idx] == 1) {
      // Encountering a new word.
      first_subword_tag_names.emplace_back(tags_as_span[idx]);
      first_subword_tag_probs.push_back(confidences_as_span[idx]);
      whole_word_token_alignments.emplace_back(token_alignments_as_span[idx]);
    } else if (!whole_word_token_alignments.empty()) {
      // Update the token alignment of the current word.
      whole_word_token_alignments.back().second =
          token_alignments_as_span[idx].second;
    }
  }
  // Add BOS and EOS sentinels.
  first_subword_tag_names.insert(first_subword_tag_names.begin(), "O");
  first_subword_tag_probs.insert(first_subword_tag_probs.begin(), 1.0);
  whole_word_token_alignments.insert(whole_word_token_alignments.begin(),
                                     {-1, -1});
  first_subword_tag_names.emplace_back("O");
  first_subword_tag_probs.push_back(1.0);
  const int last_exclusive_end = whole_word_token_alignments.back().second;
  whole_word_token_alignments.emplace_back(last_exclusive_end,
                                           last_exclusive_end);
  TFLITE_ASSIGN_OR_RETURN(
      auto slot_mentions,
      DecodeSlotChunks(first_subword_tag_names, first_subword_tag_probs,
                       whole_word_token_alignments));
  return slot_mentions;
}

}  // namespace

absl::Status SlotModulePopulateResponse(
    const std::vector<absl::string_view>& tags, const float* confidences,
    const std::vector<std::pair<int, int>>& token_alignments,
    const std::vector<int>& token_turn_ids,
    const std::vector<int>& first_subword_indicators, float threshold,
    const std::vector<absl::string_view>& reverse_utterance_list_to_encode,
    CluResponse* response) {
  if (token_alignments.size() != token_turn_ids.size()) {
    return absl::InternalError(absl::StrCat(
        "Lengths of token alignments and token_ids are not equal: "
        "tag_alignemtns size: ",
        token_alignments.size(), " turn_ids size: ", token_turn_ids.size()));
  }
  // The effective seq_len is the minimal size of tags and token_alignments.
  const size_t seq_len = std::min(tags.size(), token_alignments.size());
  const auto tags_as_span = absl::MakeConstSpan(tags).subspan(0, seq_len);
  const auto confidences_as_span = absl::MakeConstSpan(confidences, seq_len);
  const auto token_alignments_as_span =
      absl::MakeConstSpan(token_alignments).subspan(0, seq_len);
  int cur_turn_start = 0;
  while (cur_turn_start < seq_len) {
    // Determine the start and end of each turn.
    const int cur_turn_idx = token_turn_ids[cur_turn_start];
    int cur_turn_end = cur_turn_start + 1;
    while (cur_turn_end < seq_len &&
           token_turn_ids[cur_turn_end] == cur_turn_idx) {
      ++cur_turn_end;
    }

    // Prepare the data and decode slot chunks.
    std::vector<SlotMentionStruct> cur_turn_slot_mentions;
      // Decode slot chunks based on first subword tokens in the turn.
    TFLITE_ASSIGN_OR_RETURN(cur_turn_slot_mentions,
                     DecodeSlotChunksPredictOnFirstSubword(
                         cur_turn_start, cur_turn_end, seq_len, tags_as_span,
                         confidences_as_span, token_alignments_as_span,
                         first_subword_indicators));

    // Populate the response.
    for (const auto& chunk : cur_turn_slot_mentions) {
      if (chunk.start == -1 || cur_turn_idx != 0) continue;
      if (chunk.confidence < threshold) continue;
      auto slot = response->mutable_mentioned_slots()->Add();
      slot->set_slot(chunk.repr.Name());
      auto mention = slot->mutable_mention();
      mention->set_value(
          std::string(reverse_utterance_list_to_encode[cur_turn_idx].substr(
              chunk.start, chunk.exclusive_end - chunk.start)));
      mention->set_score(chunk.confidence);
      mention->set_start(chunk.start);
      mention->set_end(chunk.exclusive_end);
      if (chunk.start >= chunk.exclusive_end) {
        return absl::InternalError(
            absl::StrCat("Empty span: ", slot->DebugString()));
      }
    }

    // Set start for the next turn.
    cur_turn_start = cur_turn_end;
  }
  return absl::OkStatus();
}

}  // namespace tflite::task::text::clu
