// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/surrounding_text_tracker.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace ui {
namespace {

// Replaces the substring of the given str with the offset with replacement.
// If the range is outside of the str, str will not be updated (i.e. final
// result may not contain replacement) but at least offset should be adjusted.
void ReplaceString16WithOffset(std::u16string& str,
                               size_t& offset,
                               size_t pos,
                               size_t n,
                               const std::u16string& replacement) {
  if (offset + str.length() < pos) {
    // replacement starts after the known str. Do nothing.
    return;
  }
  if (pos + n < offset) {
    // replacement is in the range of [0:offset). Just adjust the offset.
    offset += replacement.length() - n;
    return;
  }

  // Here, we have overlap (including just concatenating) of the original
  // str and the replacement.
  if (pos < offset) {
    // Replacement starts before the current offset.
    // Merge the pattern, and adjust the offset.
    str.replace(0, n - (offset - pos), replacement);
    offset = pos;
    return;
  }

  // Here, the overlap starts the same as or after the offset.
  // In this case, offset is not changed.
  size_t begin = pos - offset;  // Begin index within str.
  str.replace(begin, std::min(n, str.length() - begin), replacement);
}

// Erases [pos:pos+n) from the given str with the offset.
void EraseString16WithOffset(std::u16string& str,
                             size_t& offset,
                             size_t pos,
                             size_t n) {
  if (offset + str.length() <= pos) {
    // The erasing range is after the str's range. Do nothing.
    return;
  }

  if (pos + n <= offset) {
    // The erasing range is included in [0:offset]. Just adjust the offset.
    offset -= n;
    return;
  }

  // Here we have to actually erase some range of str.
  if (pos < offset) {
    // The erasing range starts before the offset.
    str.erase(0, n - (offset - pos));
    offset = pos;
    return;
  }

  size_t begin = pos - offset;
  str.erase(begin, std::min(n, str.length() - begin));
}

}  // namespace

gfx::Range SurroundingTextTracker::State::GetSurroundingTextRange() const {
  return {utf16_offset, utf16_offset + surrounding_text.length()};
}

std::optional<std::u16string_view>
SurroundingTextTracker::State::GetCompositionText() const {
  if (composition.is_empty()) {
    // Represents no composition. Return empty composition text as a valid
    // result.
    return std::u16string_view();
  }

  if (!composition.IsBoundedBy(GetSurroundingTextRange())) {
    // composition range is out of the range. Return error.
    return std::nullopt;
  }

  return std::u16string_view(surrounding_text)
      .substr(composition.GetMin() - utf16_offset, composition.length());
}

SurroundingTextTracker::Entry::Entry(State state,
                                     base::RepeatingClosure command)
    : state(std::move(state)), command(std::move(command)) {}

SurroundingTextTracker::Entry::Entry(const Entry&) = default;
SurroundingTextTracker::Entry::Entry(Entry&&) = default;
SurroundingTextTracker::Entry& SurroundingTextTracker::Entry::operator=(
    const Entry&) = default;
SurroundingTextTracker::Entry& SurroundingTextTracker::Entry::operator=(
    Entry&&) = default;
SurroundingTextTracker::Entry::~Entry() = default;

SurroundingTextTracker::SurroundingTextTracker() {
  ResetInternal(u"", 0u, gfx::Range(0));
}

SurroundingTextTracker::~SurroundingTextTracker() = default;

void SurroundingTextTracker::Reset() {
  ResetInternal(u"", 0u, gfx::Range(0));
}

void SurroundingTextTracker::CancelComposition() {
  predicted_state_.composition = gfx::Range();
  // TODO(b/267944900): Determine if the expectations need to be updated as
  // well.
  expected_updates_.clear();
}

SurroundingTextTracker::UpdateResult SurroundingTextTracker::Update(
    const std::u16string_view surrounding_text,
    size_t utf16_offset,
    const gfx::Range& selection) {
  for (auto it = expected_updates_.begin(); it != expected_updates_.end();
       ++it) {
    if (it->state.selection != selection) {
      continue;
    }

    // TODO(crbug.com/40251329): Limit the trailing text to support cases
    // where trailing text is truncated.
    size_t compare_begin = std::max(utf16_offset, it->state.utf16_offset);
    std::u16string_view target =
        surrounding_text.substr(compare_begin - utf16_offset);
    std::u16string_view history =
        std::u16string_view(it->state.surrounding_text)
            .substr(compare_begin - it->state.utf16_offset);

    if (target != history) {
      continue;
    }

    // Found the target state, but it may be different from the one we
    // estimate. Because the Update may be called multiple times for the same
    // event. Check if the recorded state is exact same here to skip unneeded
    // recalculation.
    if (it->state.surrounding_text == surrounding_text &&
        it->state.utf16_offset == utf16_offset &&
        it->state.selection == selection) {
      expected_updates_.erase(expected_updates_.begin(), it);
      return UpdateResult::kUpdated;
    }

    // Otherwise, recalculate the predicts.
    predicted_state_ = State{
        std::u16string(surrounding_text), utf16_offset, selection,
        predicted_state_.composition,  // Carried from the original state.
    };

    base::RepeatingClosure current_command = std::move(it->command);
    std::vector<base::RepeatingClosure> remaining_commands;
    for (++it; it != expected_updates_.end(); ++it) {
      remaining_commands.push_back(std::move(it->command));
    }
    expected_updates_.clear();
    expected_updates_.emplace_back(predicted_state_,
                                   std::move(current_command));

    // Replay all remaining commands to re-calculate predicted states from the
    // given one.
    for (auto& command : remaining_commands) {
      command.Run();
    }
    return UpdateResult::kUpdated;
  }

  VLOG(1) << "Unknown surrounding text update is found";
  ResetInternal(surrounding_text, utf16_offset, selection);
  return UpdateResult::kReset;
}

void SurroundingTextTracker::OnSetEditableSelectionRange(
    const gfx::Range& range) {
  predicted_state_.selection = range;
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnSetEditableSelectionRange,
                          base::Unretained(this), range));
}

void SurroundingTextTracker::OnSetCompositionText(
    const ui::CompositionText& composition) {
  // If it has a composition text already, replace it.
  // Otherwise, replace (or insert) selected text.
  const gfx::Range& old_range = predicted_state_.composition.is_empty()
                                    ? predicted_state_.selection
                                    : predicted_state_.composition;
  size_t composition_begin = old_range.GetMin();
  if (old_range.GetMax() < predicted_state_.utf16_offset ||
      old_range.GetMin() > predicted_state_.utf16_offset +
                               predicted_state_.surrounding_text.length()) {
    predicted_state_.surrounding_text = composition.text;
    predicted_state_.utf16_offset = composition_begin;
  } else {
    ReplaceString16WithOffset(predicted_state_.surrounding_text,
                              predicted_state_.utf16_offset, composition_begin,
                              old_range.length(), composition.text);
  }
  predicted_state_.selection =
      gfx::Range(composition_begin + composition.selection.start(),
                 composition_begin + composition.selection.end());
  predicted_state_.composition = gfx::Range(
      composition_begin, composition_begin + composition.text.length());
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnSetCompositionText,
                          base::Unretained(this), composition));
}

void SurroundingTextTracker::OnSetCompositionFromExistingText(
    const gfx::Range& range) {
  predicted_state_.composition = range;
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(
          &SurroundingTextTracker::OnSetCompositionFromExistingText,
          base::Unretained(this), range));
}

void SurroundingTextTracker::OnConfirmCompositionText(bool keep_selection) {
  if (!predicted_state_.composition.is_empty()) {
    if (!keep_selection && !predicted_state_.composition.is_empty()) {
      predicted_state_.selection =
          gfx::Range(predicted_state_.composition.end());
    }
    predicted_state_.composition = gfx::Range();
  }
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnConfirmCompositionText,
                          base::Unretained(this), keep_selection));
}

void SurroundingTextTracker::OnClearCompositionText() {
  if (!predicted_state_.composition.is_empty()) {
    EraseString16WithOffset(predicted_state_.surrounding_text,
                            predicted_state_.utf16_offset,
                            predicted_state_.composition.GetMin(),
                            predicted_state_.composition.length());
    // Set selection to the position where composition existed.
    predicted_state_.selection =
        gfx::Range(predicted_state_.composition.GetMin());
    predicted_state_.composition = gfx::Range();
  }
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnClearCompositionText,
                          base::Unretained(this)));
}

void SurroundingTextTracker::OnInsertText(
    const std::u16string_view text,
    TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  gfx::Range rewritten_range = predicted_state_.selection;
  if (!predicted_state_.composition.is_empty()) {
    // Cancel the current composition.
    if (predicted_state_.composition.Intersects(rewritten_range)) {
      // Selection and composition has overlap, so take the union here.
      // Just after this section, the whole range will be replaced by |text|.
      rewritten_range =
          gfx::Range(std::min(predicted_state_.composition.GetMin(),
                              rewritten_range.GetMin()),
                     std::max(predicted_state_.composition.GetMax(),
                              rewritten_range.GetMax()));
    } else {
      // Otherwise, remove the composition. If the composition appears before
      // the rewritten range, the offset needs to be updated.
      EraseString16WithOffset(predicted_state_.surrounding_text,
                              predicted_state_.utf16_offset,
                              predicted_state_.composition.GetMin(),
                              predicted_state_.composition.length());
      if (rewritten_range.GetMin() > predicted_state_.composition.GetMin()) {
        rewritten_range = gfx::Range(
            rewritten_range.start() - predicted_state_.composition.length(),
            rewritten_range.end() - predicted_state_.composition.length());
      }
    }
  }

  if (rewritten_range.GetMin() >
          predicted_state_.utf16_offset +
              predicted_state_.surrounding_text.length() ||
      rewritten_range.GetMax() < predicted_state_.utf16_offset) {
    predicted_state_.surrounding_text = std::u16string(text);
    predicted_state_.utf16_offset = rewritten_range.GetMin();
  } else {
    ReplaceString16WithOffset(predicted_state_.surrounding_text,
                              predicted_state_.utf16_offset,
                              rewritten_range.GetMin(),
                              rewritten_range.length(), std::u16string(text));
  }
  predicted_state_.selection =
      cursor_behavior ==
              TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText
          ? gfx::Range(rewritten_range.GetMin() + text.length())
          : gfx::Range(rewritten_range.GetMin());
  predicted_state_.composition = gfx::Range();
  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnInsertText,
                          base::Unretained(this), text, cursor_behavior));
}

void SurroundingTextTracker::OnExtendSelectionAndDelete(size_t before,
                                                        size_t after) {
  if (before != 0 || after != 0 || !predicted_state_.selection.is_empty() ||
      !predicted_state_.composition.is_empty()) {
    gfx::Range delete_range(
        predicted_state_.selection.GetMin() -
            std::min(before, predicted_state_.selection.GetMin()),
        predicted_state_.selection.GetMax() + after);
    if (!predicted_state_.composition.is_empty()) {
      // Cancel the current composition.
      if (predicted_state_.composition.Intersects(delete_range)) {
        // Expand the delete_range to include the whole composition range,
        // if there's some overlap.
        delete_range =
            gfx::Range(std::min(predicted_state_.composition.GetMin(),
                                delete_range.GetMin()),
                       std::max(predicted_state_.composition.GetMax(),
                                delete_range.GetMax()));
      } else {
        // Otherwise, remove the composition here. If the composition appears
        // before the delete_range, the offset needs to be updated.
        EraseString16WithOffset(predicted_state_.surrounding_text,
                                predicted_state_.utf16_offset,
                                predicted_state_.composition.GetMin(),
                                predicted_state_.composition.length());
        if (delete_range.GetMin() > predicted_state_.composition.GetMin()) {
          delete_range = gfx::Range(
              delete_range.start() - predicted_state_.composition.length(),
              delete_range.end() - predicted_state_.composition.length());
        }
      }
    }

    EraseString16WithOffset(predicted_state_.surrounding_text,
                            predicted_state_.utf16_offset,
                            delete_range.GetMin(), delete_range.length());
    predicted_state_.selection = gfx::Range(delete_range.GetMin());
    predicted_state_.composition = gfx::Range();
  }

  expected_updates_.emplace_back(
      predicted_state_,
      base::BindRepeating(&SurroundingTextTracker::OnExtendSelectionAndDelete,
                          base::Unretained(this), before, after));
}

void SurroundingTextTracker::ResetInternal(std::u16string_view surrounding_text,
                                           size_t utf16_offset,
                                           const gfx::Range& selection) {
  predicted_state_ = State{std::u16string(surrounding_text), utf16_offset,
                           selection, gfx::Range()};
  expected_updates_.clear();
  expected_updates_.emplace_back(predicted_state_, base::RepeatingClosure());
}

}  // namespace ui
