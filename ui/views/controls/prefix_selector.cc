// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/prefix_selector.h"

#include <algorithm>
#include <limits>

#include "base/i18n/case_conversion.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include <vector>
#endif

namespace views {

PrefixSelector::PrefixSelector(PrefixDelegate* delegate, View* host_view)
    : prefix_delegate_(delegate),
      host_view_(host_view),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

PrefixSelector::~PrefixSelector() = default;

void PrefixSelector::OnViewBlur() {
  ClearText();
}

bool PrefixSelector::ShouldContinueSelection() const {
  const base::TimeTicks now(tick_clock_->NowTicks());
  constexpr auto kTimeBeforeClearing = base::Seconds(1);
  return (now - time_of_last_key_) < kTimeBeforeClearing;
}

base::WeakPtr<ui::TextInputClient> PrefixSelector::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefixSelector::SetCompositionText(
    const ui::CompositionText& composition) {}

size_t PrefixSelector::ConfirmCompositionText(bool keep_selection) {
  return std::numeric_limits<size_t>::max();
}

void PrefixSelector::ClearCompositionText() {}

void PrefixSelector::InsertText(const std::u16string& text,
                                InsertTextCursorBehavior cursor_behavior) {
  // TODO(crbug.com/1155331): Handle |cursor_behavior| correctly.
  OnTextInput(text);
}

void PrefixSelector::InsertChar(const ui::KeyEvent& event) {
  OnTextInput(std::u16string(1, event.GetCharacter()));
}

ui::TextInputType PrefixSelector::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_TEXT;
}

ui::TextInputMode PrefixSelector::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection PrefixSelector::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

int PrefixSelector::GetTextInputFlags() const {
  return 0;
}

bool PrefixSelector::CanComposeInline() const {
  return false;
}

gfx::Rect PrefixSelector::GetCaretBounds() const {
  gfx::Rect rect(host_view_->GetVisibleBounds().origin(), gfx::Size());
  // TextInputClient::GetCaretBounds is expected to return a value in screen
  // coordinates.
  views::View::ConvertRectToScreen(host_view_, &rect);
  return rect;
}

gfx::Rect PrefixSelector::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool PrefixSelector::GetCompositionCharacterBounds(size_t index,
                                                   gfx::Rect* rect) const {
  // TextInputClient::GetCompositionCharacterBounds is expected to fill |rect|
  // in screen coordinates and GetCaretBounds returns screen coordinates.
  *rect = GetCaretBounds();
  return false;
}

bool PrefixSelector::HasCompositionText() const {
  return false;
}

ui::TextInputClient::FocusReason PrefixSelector::GetFocusReason() const {
  // TODO(https://crbug.com/824604): Implement this correctly.
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool PrefixSelector::GetTextRange(gfx::Range* range) const {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::GetCompositionTextRange(gfx::Range* range) const {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::GetEditableSelectionRange(gfx::Range* range) const {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::SetEditableSelectionRange(const gfx::Range& range) {
  return false;
}

#if BUILDFLAG(IS_MAC)
bool PrefixSelector::DeleteRange(const gfx::Range& range) {
  return false;
}
#endif

bool PrefixSelector::GetTextFromRange(const gfx::Range& range,
                                      std::u16string* text) const {
  return false;
}

void PrefixSelector::OnInputMethodChanged() {
  ClearText();
}

bool PrefixSelector::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return true;
}

void PrefixSelector::ExtendSelectionAndDelete(size_t before, size_t after) {}

void PrefixSelector::EnsureCaretNotInRect(const gfx::Rect& rect) {}

bool PrefixSelector::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

void PrefixSelector::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {}

ukm::SourceId PrefixSelector::GetClientSourceForMetrics() const {
  // TODO(shend): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::SourceId();
}

bool PrefixSelector::ShouldDoLearning() {
  // TODO(https://crbug.com/311180): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool PrefixSelector::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  // TODO(crbug.com/40623107): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
gfx::Range PrefixSelector::GetAutocorrectRange() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Range();
}

gfx::Rect PrefixSelector::GetAutocorrectCharacterBounds() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool PrefixSelector::SetAutocorrectRange(const gfx::Range& range) {
  // TODO(crbug.com/40134032): Implement SetAutocorrectRange.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}
#endif

#if BUILDFLAG(IS_WIN)
void PrefixSelector::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
void PrefixSelector::GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {}
#endif

void PrefixSelector::OnTextInput(const std::u16string& text) {
  // Small hack to filter out 'tab' and 'enter' input, as the expectation is
  // that they are control characters and will not affect the currently-active
  // prefix.
  if (text.length() == 1 &&
      (text[0] == L'\t' || text[0] == L'\r' || text[0] == L'\n'))
    return;

  const size_t row_count = prefix_delegate_->GetRowCount();
  if (row_count == 0)
    return;

  // Search for |text| if it has been a while since the user typed, otherwise
  // append |text| to |current_text_| and search for that. If it has been a
  // while search after the current row, otherwise search starting from the
  // current row.
  size_t row = prefix_delegate_->GetSelectedRow().value_or(0);
  if (ShouldContinueSelection()) {
    current_text_ += text;
  } else {
    current_text_ = text;
    if (prefix_delegate_->GetSelectedRow().has_value())
      row = (row + 1) % row_count;
  }
  time_of_last_key_ = tick_clock_->NowTicks();

  const size_t start_row = row;
  const std::u16string lower_text(base::i18n::ToLower(current_text_));
  do {
    if (TextAtRowMatchesText(row, lower_text)) {
      prefix_delegate_->SetSelectedRow(row);
      return;
    }
    row = (row + 1) % row_count;
  } while (row != start_row);
}

bool PrefixSelector::TextAtRowMatchesText(size_t row,
                                          const std::u16string& lower_text) {
  const std::u16string model_text(
      base::i18n::ToLower(prefix_delegate_->GetTextForRow(row)));
  return (model_text.size() >= lower_text.size()) &&
         (model_text.compare(0, lower_text.size(), lower_text) == 0);
}

void PrefixSelector::ClearText() {
  current_text_.clear();
  time_of_last_key_ = base::TimeTicks();
}

}  // namespace views
