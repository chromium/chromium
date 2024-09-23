// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_SURROUNDING_TEXT_TRACKER_H_
#define UI_BASE_IME_SURROUNDING_TEXT_TRACKER_H_

#include <deque>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/range/range.h"

namespace ui {

struct CompositionText;

// Tracks the surrounding text. Because IMF works in an asynchronous manner,
// there will be timing gap that IM-Engine sends some request to the text input
// client, and the commit is actually notified.
// This class gives it a try to fill the gap by caching and emulating the
// latest surrounding text under the assumption that text editing requests are
// processed in a common manner.
class COMPONENT_EXPORT(UI_BASE_IME) SurroundingTextTracker {
 public:
  struct COMPONENT_EXPORT(UI_BASE_IME) State {
    // Returns the range of the surrounding text in UTF-16.
    gfx::Range GetSurroundingTextRange() const;

    // Returns the string piece of the composition range of the
    // |surrounding_text|.
    // If composition is out of the range, nullopt will be returned.
    std::optional<std::u16string_view> GetCompositionText() const;

    // Whole surrounding text, specifically this may include composition text.
    std::u16string surrounding_text;

    // Offset of the surrounding_text within the text input client.
    // This does not affect to either selection nor composition.
    size_t utf16_offset;

    // Selection range. If it is empty, it means the cursor. Must not be
    // InvalidRange. Must be fit in |surrounding_text| range.
    gfx::Range selection;

    // Composition range if it has. Maybe empty if there's no composition text.
    // Must not be InvalidRange. Must be fit in |surrounding_text| range.
    gfx::Range composition;
  };

  // The initial state is no-surrounding text, cursor at 0, and no composition.
  SurroundingTextTracker();
  SurroundingTextTracker(const SurroundingTextTracker&) = delete;
  SurroundingTextTracker& operator=(const SurroundingTextTracker&) = delete;
  ~SurroundingTextTracker();

  const State& predicted_state() const { return predicted_state_; }

  // Resets the internal state, including composition state, surrounding text
  // and held histories. Used when the entire state needs to be reset.
  // TODO(b/267944900): Investigate if this is still needed once
  // kWaylandCancelComposition flag is enabled by default.
  void Reset();

  // Resets only the composition state and held histories.
  // Used when only the composition state is cancelled by the input field.
  void CancelComposition();

  enum class UpdateResult {
    // Expected update entry is found in |expected_updates_|.
    kUpdated,
    // No update entry corresponding to the given arguments is found.
    // All the states are reset to the given arguments.
    kReset,
  };
  // Expected to be called on surrounding text update event from text input
  // client. If there was some known state matching to the arguments,
  // forgets the state histories before it, and returns kUpdateFoundInHistory.
  // Otherwise, forgets everything and reset by the state of the given
  // arguments, then returns kHistoryIsReset.
  // Note intentiontally ignored composition text.
  UpdateResult Update(const std::u16string_view surrounding_text,
                      size_t utf16_offset,
                      const gfx::Range& selection);

  // The following methods are used to guess new surrounding text state.
  // See TextInputClient for detailed behavior.
  void OnSetEditableSelectionRange(const gfx::Range& range);
  void OnSetCompositionText(const ui::CompositionText& composition);
  void OnSetCompositionFromExistingText(const gfx::Range& range);
  void OnConfirmCompositionText(bool keep_selection);
  void OnClearCompositionText();
  void OnInsertText(const std::u16string_view text,
                    TextInputClient::InsertTextCursorBehavior cursor_behavior);
  void OnExtendSelectionAndDelete(size_t before, size_t after);

 private:
  // History of events and their expected states.
  struct Entry {
    State state;
    base::RepeatingClosure command;

    Entry(State state, base::RepeatingClosure command);

    // Copy/Move-able.
    Entry(const Entry&);
    Entry(Entry&& entry);
    Entry& operator=(const Entry&);
    Entry& operator=(Entry&&);

    ~Entry();
  };

  void ResetInternal(std::u16string_view surrounding_text,
                     size_t utf16_offset,
                     const gfx::Range& selection);

  // The latest known state.
  State predicted_state_;
  std::deque<Entry> expected_updates_;
};

}  // namespace ui

#endif  // UI_BASE_IME_SURROUNDING_TEXT_TRACKER_H_
