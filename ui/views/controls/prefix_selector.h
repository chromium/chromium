// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
#define UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_

#include <stddef.h>
#include <stdint.h>

#if defined(OS_WIN)
#include <vector>
#endif

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/views_export.h"

namespace base {
class TickClock;
}

namespace views {

class PrefixDelegate;
class View;

// PrefixSelector is used to change the selection in a view as the user
// types characters.
class VIEWS_EXPORT PrefixSelector : public ui::TextInputClient {
 public:
  PrefixSelector(PrefixDelegate* delegate, View* host_view);
  ~PrefixSelector() override;

  // Invoked from the view when it loses focus.
  void OnViewBlur();

  // Returns whether a key typed now would continue the existing search or start
  // a new search.
  bool ShouldContinueSelection() const;

  // ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  uint32_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;

  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
#endif

#if defined(OS_WIN)
  void GetActiveTextInputControlLayoutBounds(
      base::Optional<gfx::Rect>* control_bounds,
      base::Optional<gfx::Rect>* selection_bounds) override;
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const base::string16& active_composition_text,
      bool is_composition_committed) override;
#endif

  void set_tick_clock_for_testing(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

 private:
  // Invoked when text is typed. Tries to change the selection appropriately.
  void OnTextInput(const base::string16& text);

  // Returns true if the text of the node at |row| starts with |lower_text|.
  bool TextAtRowMatchesText(int row, const base::string16& lower_text);

  // Clears |current_text_| and resets |time_of_last_key_|.
  void ClearText();

  PrefixDelegate* prefix_delegate_;

  View* host_view_;

  // Time OnTextInput() was last invoked.
  base::TimeTicks time_of_last_key_;

  base::string16 current_text_;

  // TickClock used for getting the time of the current keystroke, used for
  // continuing or restarting selections.
  const base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(PrefixSelector);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
