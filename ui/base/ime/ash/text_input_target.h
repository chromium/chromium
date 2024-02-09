// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_TEXT_INPUT_TARGET_H_
#define UI_BASE_IME_ASH_TEXT_INPUT_TARGET_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ash {

struct SurroundingTextInfo {
  std::u16string surrounding_text;

  // This is relative to the beginning of |surrounding_text|.
  gfx::Range selection_range;

  // Offset of the surrounding_text in the field in UTF-16.
  size_t offset;
};

// An interface representing an input target that supports text editing via a
// TextInputMethod. Applications like Chrome browser, Android apps, Linux apps
// should all implement this interface in order to support TextInputMethods.
// All strings related to IME operations should be UTF-16 encoded and all
// indices/ranges relative to those strings should be UTF-16 code units.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) TextInputTarget {
 public:
  using SetAutocorrectRangeDoneCallback = base::OnceCallback<void(bool)>;

  // Called when the engine commit a text.
  virtual void CommitText(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) = 0;

  // Called when the engine changes the composition range.
  // Returns true if the operation was successful.
  // If |text_spans| is empty, then this function uses a default span that
  // spans across the new composition range.
  virtual bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) = 0;
  virtual bool SetComposingRange(
      uint32_t start,
      uint32_t end,
      const std::vector<ui::ImeTextSpan>& text_spans) = 0;
  virtual gfx::Range GetAutocorrectRange() = 0;

  // Sets the autocorrect range to be `range`.
  // Actual implementation must call |callback| and notify if the autocorrect
  // range is set successfully.
  virtual void SetAutocorrectRange(
      const gfx::Range& range,
      SetAutocorrectRangeDoneCallback callback) = 0;
  virtual std::optional<ui::GrammarFragment> GetGrammarFragmentAtCursor() = 0;
  virtual bool ClearGrammarFragments(const gfx::Range& range) = 0;
  virtual bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragements) = 0;

  // Called when the engine updates composition text.
  virtual void UpdateCompositionText(const ui::CompositionText& text,
                                     uint32_t cursor_pos,
                                     bool visible) = 0;

  // Called when the engine request deleting surrounding string.
  virtual void DeleteSurroundingText(uint32_t num_char16s_before_cursor,
                                     uint32_t num_char16s_after_cursor) = 0;

  // Deletes any active composition, and the current selection plus the
  // specified number of char16 values before and after the selection, and
  // replaces it with |replacement_string|.
  // Places the cursor at the end of |replacement_string|.
  virtual void ReplaceSurroundingText(uint32_t length_before_selection,
                                      uint32_t length_after_selection,
                                      std::u16string_view replacement_text) = 0;

  // Called from the extension API.
  // WARNING: This could return a stale cache that doesn't reflect reality, due
  // to async-ness between browser-process IMF and render-process
  // `ui::TextInputClient`.
  // TODO(crbug/1194424): Ensure this always returns accurate result.
  virtual SurroundingTextInfo GetSurroundingTextInfo() = 0;

  // Called when the engine sends a key event.
  virtual void SendKeyEvent(ui::KeyEvent* event) = 0;

  // Gets the input method pointer.
  virtual ui::InputMethod* GetInputMethod() = 0;

  // Commits the current composition and keeps the selection unchanged.
  // Set |reset_engine| to false if this was triggered from the extension.
  virtual void ConfirmComposition(bool reset_engine) = 0;

  // Returns true if there is any composition text.
  virtual bool HasCompositionText() = 0;

  // Returns the ukm::SourceId that identifies the currently focused client.
  virtual ukm::SourceId GetClientSourceForMetrics() = 0;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_TEXT_INPUT_TARGET_H_
