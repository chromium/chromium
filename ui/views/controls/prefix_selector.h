// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
#define UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

  PrefixSelector(const PrefixSelector&) = delete;
  PrefixSelector& operator=(const PrefixSelector&) = delete;

  ~PrefixSelector() override;

  // Invoked from the view when it loses focus.
  void OnViewBlur();

  // Returns whether a key typed now would continue the existing search or start
  // a new search.
  bool ShouldContinueSelection() const;

  // ui::TextInputClient:
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const ui::CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
  bool GetCompositionCharacterBounds(size_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
#if BUILDFLAG(IS_MAC)
  bool DeleteRange(const gfx::Range& range) override;
#endif
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;

  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  void GetActiveTextInputControlLayoutBounds(
      std::optional<gfx::Rect>* control_bounds,
      std::optional<gfx::Rect>* selection_bounds) override;
#endif

#if BUILDFLAG(IS_WIN)
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const std::u16string& active_composition_text,
      bool is_composition_committed) override;
#endif

  void set_tick_clock_for_testing(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

 private:
  // Invoked when text is typed. Tries to change the selection appropriately.
  void OnTextInput(const std::u16string& text);

  // Returns true if the text of the node at |row| starts with |lower_text|.
  bool TextAtRowMatchesText(size_t row, const std::u16string& lower_text);

  // Clears |current_text_| and resets |time_of_last_key_|.
  void ClearText();

  raw_ptr<PrefixDelegate> prefix_delegate_;

  raw_ptr<View> host_view_;

  // Time OnTextInput() was last invoked.
  base::TimeTicks time_of_last_key_;

  std::u16string current_text_;

  // TickClock used for getting the time of the current keystroke, used for
  // continuing or restarting selections.
  raw_ptr<const base::TickClock> tick_clock_;

  base::WeakPtrFactory<PrefixSelector> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
