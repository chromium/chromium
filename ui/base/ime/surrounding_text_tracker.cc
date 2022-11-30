// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/surrounding_text_tracker.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"

namespace ui {

SurroundingTextTracker::SurroundingTextTracker()
    : predicted_state_{u"", gfx::Range(0), gfx::Range()} {}

SurroundingTextTracker::~SurroundingTextTracker() = default;

void SurroundingTextTracker::Reset() {
  predicted_state_ = State{u"", gfx::Range(0), gfx::Range()};
  expected_updates_.clear();
}

SurroundingTextTracker::UpdateResult SurroundingTextTracker::Update(
    const base::StringPiece16 surrounding_text,
    const gfx::Range& selection) {
  for (auto it = expected_updates_.begin(); it != expected_updates_.end();
       ++it) {
    if (it->surrounding_text == surrounding_text &&
        it->selection == selection) {
      // Found the target state. Remove the older histories.
      // Keep the last entry, because sometimes it is notified multiple times
      // by client apps.
      expected_updates_.erase(expected_updates_.begin(), it);
      return UpdateResult::kUpdated;
    }
  }

  VLOG(1) << "Unknown surrounding text update is found";
  predicted_state_ =
      State{std::u16string(surrounding_text), selection, gfx::Range()};
  expected_updates_.clear();
  expected_updates_.push_back(predicted_state_);
  return UpdateResult::kReset;
}

void SurroundingTextTracker::OnSetEditableSelectionRange(
    const gfx::Range& range) {
  predicted_state_.selection = range;
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnSetCompositionText(
    const ui::CompositionText& composition) {
  // If it has a composition text already, replace it.
  // Otherwise, replace (or insert) selected text.
  const gfx::Range& old_range = predicted_state_.composition.is_empty()
                                    ? predicted_state_.selection
                                    : predicted_state_.composition;
  size_t composition_begin = old_range.GetMin();
  predicted_state_.surrounding_text.replace(
      composition_begin, old_range.length(), composition.text);
  predicted_state_.selection =
      gfx::Range(composition_begin + composition.selection.start(),
                 composition_begin + composition.selection.end());
  predicted_state_.composition = gfx::Range(
      composition_begin, composition_begin + composition.text.length());
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnSetCompositionFromExistingText(
    const gfx::Range& range) {
  predicted_state_.composition = range;
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnConfirmCompositionText(bool keep_selection) {
  if (predicted_state_.composition.is_empty())
    return;

  if (!keep_selection && !predicted_state_.composition.is_empty())
    predicted_state_.selection = gfx::Range(predicted_state_.composition.end());
  predicted_state_.composition = gfx::Range();
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnClearCompositionText() {
  if (predicted_state_.composition.is_empty())
    return;

  predicted_state_.surrounding_text.erase(
      predicted_state_.composition.GetMin(),
      predicted_state_.composition.length());
  // Set selection to the position where composition existed.
  predicted_state_.selection =
      gfx::Range(predicted_state_.composition.GetMin());
  predicted_state_.composition = gfx::Range();
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnInsertText(
    const base::StringPiece16 text,
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
      predicted_state_.surrounding_text.erase(
          predicted_state_.composition.GetMin(),
          predicted_state_.composition.length());
      if (rewritten_range.GetMin() > predicted_state_.composition.GetMin()) {
        rewritten_range = gfx::Range(
            rewritten_range.start() - predicted_state_.composition.length(),
            rewritten_range.end() - predicted_state_.composition.length());
      }
    }
  }

  predicted_state_.surrounding_text.replace(
      rewritten_range.GetMin(), rewritten_range.length(), std::u16string(text));
  predicted_state_.selection =
      cursor_behavior ==
              TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText
          ? gfx::Range(rewritten_range.GetMin() + text.length())
          : gfx::Range(rewritten_range.GetMin());
  predicted_state_.composition = gfx::Range();
  expected_updates_.push_back(predicted_state_);
}

void SurroundingTextTracker::OnExtendSelectionAndDelete(size_t before,
                                                        size_t after) {
  if (before == 0 && after == 0 && predicted_state_.selection.is_empty() &&
      predicted_state_.composition.is_empty()) {
    // Nothing happens for null deletion.
    return;
  }

  gfx::Range delete_range(
      predicted_state_.selection.GetMin() -
          std::min(before, predicted_state_.selection.GetMin()),
      std::min(predicted_state_.selection.GetMax() + after,
               predicted_state_.surrounding_text.length()));
  if (!predicted_state_.composition.is_empty()) {
    // Cancel the current composition.
    if (predicted_state_.composition.Intersects(delete_range)) {
      // Expand the delete_range to include the whole composition range,
      // if there's some overlap.
      delete_range = gfx::Range(std::min(predicted_state_.composition.GetMin(),
                                         delete_range.GetMin()),
                                std::max(predicted_state_.composition.GetMax(),
                                         delete_range.GetMax()));
    } else {
      // Otherwise, remove the composition here. If the composition appears
      // before the delete_range, the offset needs to be updated.
      predicted_state_.surrounding_text.erase(
          predicted_state_.composition.GetMin(),
          predicted_state_.composition.length());
      if (delete_range.GetMin() > predicted_state_.composition.GetMin()) {
        delete_range = gfx::Range(
            delete_range.start() - predicted_state_.composition.length(),
            delete_range.end() - predicted_state_.composition.length());
      }
    }
  }

  predicted_state_.surrounding_text.erase(delete_range.GetMin(),
                                          delete_range.length());
  predicted_state_.selection = gfx::Range(delete_range.GetMin());
  predicted_state_.composition = gfx::Range();
  expected_updates_.push_back(predicted_state_);
}

}  // namespace ui
