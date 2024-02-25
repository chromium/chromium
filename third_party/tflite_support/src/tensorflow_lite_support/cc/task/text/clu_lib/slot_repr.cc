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

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "absl/strings/match.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "absl/strings/strip.h"  // from @com_google_absl
#include "absl/strings/substitute.h"  // from @com_google_absl
#include "absl/types/span.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/constants.h"

namespace tflite::task::text::clu {

using ::absl::StatusOr;

// SlotRepr

std::string SlotRepr::FullName() const {
  if (domain_.empty()) return name_;
  return absl::StrCat(domain_, kNamespaceDelim, name_);
}

StatusOr<std::tuple<absl::string_view, absl::string_view>>
SlotRepr::SplitDomainAndName(const absl::string_view full_name) {
  std::vector<absl::string_view> splits =
      absl::StrSplit(full_name, kNamespaceDelim);
  if (splits.size() > 2) {
    return absl::InternalError(absl::StrCat("invalid input: ", full_name));
  }
  absl::string_view domain = "";
  absl::string_view name;
  if (splits.size() == 2) domain = splits[0];
  name = splits[splits.size() - 1];
  return std::tuple<absl::string_view, absl::string_view>{domain, name};
}

StatusOr<SlotRepr> SlotRepr::CreateFromIob(const absl::string_view repr) {
  SlotRepr ret;
  if (IsO(repr)) return ret;
  absl::string_view full_name;
  if (absl::StartsWith(repr, kSlotBTagPrefix)) {
    full_name = absl::StripPrefix(repr, kSlotBTagPrefix);
  } else if (absl::StartsWith(repr, kSlotITagPrefix)) {
    full_name = absl::StripPrefix(repr, kSlotITagPrefix);
  } else {
    return absl::InternalError(absl::StrCat("repr not started with ",
                                            kSlotBTagPrefix, " or ",
                                            kSlotITagPrefix, ": ", repr));
  }
  TFLITE_ASSIGN_OR_RETURN(const auto domain_name_pair, SplitDomainAndName(full_name));
  ret.domain_ = std::string(std::get<0>(domain_name_pair));
  ret.name_ = std::string(std::get<1>(domain_name_pair));
  return ret;
}

SlotRepr SlotRepr::Create(absl::string_view name, absl::string_view domain,
                          const bool share_across_domains) {
  SlotRepr ret;
  ret.name_ = std::string(name);
  if (!share_across_domains) {
    ret.domain_ = std::string(domain);
  }
  return ret;
}

bool SlotRepr::IsI(const absl::string_view repr) {
  return absl::StartsWith(repr, kSlotITagPrefix);
}

bool SlotRepr::IsB(const absl::string_view repr) {
  return absl::StartsWith(repr, kSlotBTagPrefix);
}

bool SlotRepr::IsO(const absl::string_view repr) { return repr == kSlotOTag; }

bool SlotRepr::operator==(const SlotRepr& other) const {
  return domain_ == other.domain_ && name_ == other.name_;
}

std::ostream& operator<<(std::ostream& os, const SlotRepr& slot_repr) {
  os << slot_repr.FullName();
  return os;
}

absl::Status ResolveInconsistentIobTagSeq(std::vector<std::string>* tag_names) {
  // Force the BOS and EOS elements to be O during prediction. Usually the
  // training takes care of it but it doesn't hurt to force them. Disable for
  // TFLite as tf.tensor_scatter_update() isn't supported well.
  if (!tag_names->empty()) {
    (*tag_names)[0] = kSlotOTag;
    (*tag_names)[tag_names->size() - 1] = kSlotOTag;
  }
  absl::string_view prev_tag = kSlotOTag;
  for (size_t i = 0; i < tag_names->size(); ++i) {
    const auto& tag = tag_names->at(i);
    if (SlotRepr::IsI(tag)) {
      TFLITE_ASSIGN_OR_RETURN(const SlotRepr repr, SlotRepr::CreateFromIob(tag));
      if (SlotRepr::IsO(prev_tag)) {
        // inconsistent case. eg.   O I-time
        (*tag_names)[i] = repr.BTag();
      } else {
        TFLITE_ASSIGN_OR_RETURN(const SlotRepr prev_repr,
                         SlotRepr::CreateFromIob(prev_tag));
        if (prev_repr.FullName() != repr.FullName()) {
          // inconsistent case. eg.   B-time I-per    I-time I-per
          (*tag_names)[i] = repr.BTag();
        }
      }
    }
    prev_tag = tag;
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<SlotMentionStruct>> DecodeSlotChunks(
    const absl::Span<const absl::string_view> tag_names,
    const absl::Span<const float> tag_probs,
    const absl::Span<const std::pair<int, int>> token_alignments) {
  if (tag_names.size() != tag_probs.size()) {
    return absl::InternalError(absl::StrCat(
        "Lengths of tag sequence and probability sequence are not equal: "
        "tag_seq size: ",
        tag_names.size(), " tag_probs size: ", tag_probs.size()));
  }
  // The index for one past the final token (including BOS and EOS)
  const size_t eos_exclusive_idx =
      std::min(tag_probs.size(), token_alignments.size());
  // Make a copy since the input is constant while still modifications are
  // needed.
  std::vector<std::string> tag_names_fixed(tag_names.begin(), tag_names.end());
  TFLITE_RETURN_IF_ERROR(ResolveInconsistentIobTagSeq(&tag_names_fixed));

  std::vector<SlotMentionStruct> result;
  // Compute slot tag spans
  // Current state
  int cur_slot_start = -1;
  int cur_slot_exclusive_end = -1;
  float cur_slot_confidence = 1.0;
  SlotRepr cur_slot;
  for (int token_i = 0; token_i < eos_exclusive_idx; ++token_i) {
    const auto& tag_str_i = tag_names_fixed.at(token_i);
    // I tag
    if (SlotRepr::IsI(tag_str_i)) {
      SlotRepr slot_tag_i;
      TFLITE_ASSIGN_OR_RETURN(slot_tag_i, SlotRepr::CreateFromIob(tag_str_i));
      if (cur_slot == slot_tag_i) {
        cur_slot_exclusive_end = token_i + 1;
        // Compute the phrase level confidence by taking min(tag confidences).
        cur_slot_confidence = std::min(cur_slot_confidence, tag_probs[token_i]);
        continue;
      } else {
        return ::absl::InvalidArgumentError(absl::StrCat(
            "Bad sequence at: '", cur_slot.FullName(), "', '", token_i, "'"));
      }
    }
    // Emit
    if (!cur_slot.Name().empty()) {
      if (cur_slot_start == -1) {
        return absl::InternalError("cur_slot_start should not be -1");
      }
      if (cur_slot_exclusive_end == -1) {
        return absl::InternalError("cur_slot_exclusive_end should not be -1");
      }
      result.emplace_back(
          SlotMentionStruct{cur_slot, token_alignments[cur_slot_start].first,
                            token_alignments[cur_slot_exclusive_end - 1].second,
                            cur_slot_confidence});
    }
    // B tag
    if (SlotRepr::IsB(tag_str_i)) {
      cur_slot_start = token_i;
      cur_slot_exclusive_end = token_i + 1;
      cur_slot_confidence = tag_probs[token_i];
      TFLITE_ASSIGN_OR_RETURN(cur_slot, SlotRepr::CreateFromIob(tag_str_i));
    } else {
      // O tag
      if (!SlotRepr::IsO(tag_str_i)) {
        return absl::InternalError(
            absl::StrCat("Bad sequence at: ", tag_str_i));
      }
      // Clear state
      cur_slot_start = -1;
      cur_slot_exclusive_end = -1;
      cur_slot_confidence = 1.0;
      cur_slot = SlotRepr();
    }
  }
  // Emit
  if (!cur_slot.Name().empty()) {
    if (cur_slot_start == -1) {
      return absl::InternalError("cur_slot_start should not be -1");
    }
    if (cur_slot_exclusive_end == -1) {
      return absl::InternalError("cur_slot_exclusive_end should not be -1");
    }
    result.emplace_back(
        SlotMentionStruct{cur_slot, token_alignments[cur_slot_start].first,
                          token_alignments[cur_slot_exclusive_end - 1].second,
                          cur_slot_confidence});
  }
  return result;
}

}  // namespace tflite::task::text::clu
