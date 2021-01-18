// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fake_text_input_client.h"

#include "base/check_op.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

FakeTextInputClient::FakeTextInputClient(TextInputType text_input_type)
    : text_input_type_(text_input_type) {}

FakeTextInputClient::~FakeTextInputClient() = default;

void FakeTextInputClient::set_text_input_type(TextInputType text_input_type) {
  text_input_type_ = text_input_type;
}

void FakeTextInputClient::SetTextAndSelection(const base::string16& text,
                                              gfx::Range selection) {
  DCHECK_LE(selection_.end(), text.length());
  text_ = text;
  selection_ = selection;
}

void FakeTextInputClient::SetCompositionText(
    const CompositionText& composition) {}

uint32_t FakeTextInputClient::ConfirmCompositionText(bool keep_selection) {
  return UINT32_MAX;
}

void FakeTextInputClient::ClearCompositionText() {}

void FakeTextInputClient::InsertText(
    const base::string16& text,
    TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  text_.insert(selection_.start(), text);

  if (cursor_behavior ==
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText) {
    selection_ = gfx::Range(selection_.start() + text.length(),
                            selection_.end() + text.length());
  }
}

void FakeTextInputClient::InsertChar(const KeyEvent& event) {}

TextInputType FakeTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

TextInputMode FakeTextInputClient::GetTextInputMode() const {
  return TEXT_INPUT_MODE_NONE;
}

base::i18n::TextDirection FakeTextInputClient::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

int FakeTextInputClient::GetTextInputFlags() const {
  return EF_NONE;
}

bool FakeTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect FakeTextInputClient::GetCaretBounds() const {
  return {};
}

bool FakeTextInputClient::GetCompositionCharacterBounds(uint32_t index,
                                                        gfx::Rect* rect) const {
  return false;
}

bool FakeTextInputClient::HasCompositionText() const {
  return false;
}

ui::TextInputClient::FocusReason FakeTextInputClient::GetFocusReason() const {
  return ui::TextInputClient::FOCUS_REASON_NONE;
}

bool FakeTextInputClient::GetTextRange(gfx::Range* range) const {
  *range = gfx::Range(0, text_.length());
  return true;
}

bool FakeTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool FakeTextInputClient::GetEditableSelectionRange(gfx::Range* range) const {
  *range = selection_;
  return true;
}

bool FakeTextInputClient::SetEditableSelectionRange(const gfx::Range& range) {
  return false;
}

bool FakeTextInputClient::DeleteRange(const gfx::Range& range) {
  return false;
}

bool FakeTextInputClient::GetTextFromRange(const gfx::Range& range,
                                           base::string16* text) const {
  return false;
}

void FakeTextInputClient::OnInputMethodChanged() {}

bool FakeTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

void FakeTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                   size_t after) {}

void FakeTextInputClient::EnsureCaretNotInRect(const gfx::Rect& rect) {}

bool FakeTextInputClient::IsTextEditCommandEnabled(
    TextEditCommand command) const {
  return false;
}

void FakeTextInputClient::SetTextEditCommandForNextKeyEvent(
    TextEditCommand command) {}

ukm::SourceId FakeTextInputClient::GetClientSourceForMetrics() const {
  return {};
}

bool FakeTextInputClient::ShouldDoLearning() {
  return false;
}

#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
bool FakeTextInputClient::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  if (range.start() < 0 || range.end() > text_.length())
    return false;

  composition_range_ = range;
  ime_text_spans_ = ui_ime_text_spans;
  return true;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Range FakeTextInputClient::GetAutocorrectRange() const {
  return {};
}

gfx::Rect FakeTextInputClient::GetAutocorrectCharacterBounds() const {
  return {};
}

bool FakeTextInputClient::SetAutocorrectRange(const gfx::Range& range) {
  return false;
}
#endif

#if defined(OS_WIN)
void FakeTextInputClient::GetActiveTextInputControlLayoutBounds(
    base::Optional<gfx::Rect>* control_bounds,
    base::Optional<gfx::Rect>* selection_bounds) {}

void FakeTextInputClient::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const base::string16& active_composition_text,
    bool is_composition_committed) {}
#endif

}  // namespace ui
