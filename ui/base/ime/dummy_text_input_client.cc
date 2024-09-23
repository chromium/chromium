// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_text_input_client.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

DummyTextInputClient::DummyTextInputClient()
    : DummyTextInputClient(TEXT_INPUT_TYPE_NONE) {}

DummyTextInputClient::DummyTextInputClient(TextInputType text_input_type)
    : DummyTextInputClient(text_input_type, TEXT_INPUT_MODE_DEFAULT) {}

DummyTextInputClient::DummyTextInputClient(TextInputType text_input_type,
                                           TextInputMode text_input_mode)
    : text_input_type_(text_input_type),
      text_input_mode_(text_input_mode),
      insert_char_count_(0),
      autocorrect_enabled_(true) {}

DummyTextInputClient::~DummyTextInputClient() {
}

base::WeakPtr<ui::TextInputClient> DummyTextInputClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DummyTextInputClient::SetCompositionText(
    const CompositionText& composition) {
  composition_history_.push_back(composition);
}

size_t DummyTextInputClient::ConfirmCompositionText(bool keep_selection) {
  return std::numeric_limits<size_t>::max();
}

void DummyTextInputClient::ClearCompositionText() {
  SetCompositionText(CompositionText());
}

void DummyTextInputClient::InsertText(
    const std::u16string& text,
    InsertTextCursorBehavior cursor_behavior) {
  insert_text_history_.push_back(text);
}

void DummyTextInputClient::InsertChar(const KeyEvent& event) {
  ++insert_char_count_;
  last_insert_char_ = event.GetCharacter();
}

TextInputType DummyTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

TextInputMode DummyTextInputClient::GetTextInputMode() const {
  return text_input_mode_;
}

base::i18n::TextDirection DummyTextInputClient::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

int DummyTextInputClient::GetTextInputFlags() const {
  return 0;
}

bool DummyTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect DummyTextInputClient::GetCaretBounds() const {
  return gfx::Rect();
}

gfx::Rect DummyTextInputClient::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool DummyTextInputClient::GetCompositionCharacterBounds(
    size_t index,
    gfx::Rect* rect) const {
  return false;
}

bool DummyTextInputClient::HasCompositionText() const {
  return false;
}

ui::TextInputClient::FocusReason DummyTextInputClient::GetFocusReason() const {
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool DummyTextInputClient::GetTextRange(gfx::Range* range) const {
  return false;
}

bool DummyTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool DummyTextInputClient::GetEditableSelectionRange(gfx::Range* range) const {
  if (!cursor_range_.IsValid())
    return false;
  range->set_start(cursor_range_.start());
  range->set_end(cursor_range_.end());
  return true;
}

bool DummyTextInputClient::SetEditableSelectionRange(const gfx::Range& range) {
  selection_history_.push_back(range);
  cursor_range_ = range;
  return true;
}

#if BUILDFLAG(IS_MAC)
bool DummyTextInputClient::DeleteRange(const gfx::Range& range) {
  return false;
}
#endif

bool DummyTextInputClient::GetTextFromRange(const gfx::Range& range,
                                            std::u16string* text) const {
  return false;
}

void DummyTextInputClient::OnInputMethodChanged() {
}

bool DummyTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

void DummyTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                    size_t after) {
}

void DummyTextInputClient::EnsureCaretNotInRect(const gfx::Rect& rect) {}

bool DummyTextInputClient::IsTextEditCommandEnabled(
    TextEditCommand command) const {
  return false;
}

void DummyTextInputClient::SetTextEditCommandForNextKeyEvent(
    TextEditCommand command) {}

ukm::SourceId DummyTextInputClient::GetClientSourceForMetrics() const {
  return ukm::SourceId{};
}

bool DummyTextInputClient::ShouldDoLearning() {
  return false;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool DummyTextInputClient::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  return false;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
gfx::Range DummyTextInputClient::GetAutocorrectRange() const {
  return autocorrect_range_;
}
gfx::Rect DummyTextInputClient::GetAutocorrectCharacterBounds() const {
  return gfx::Rect();
}

bool DummyTextInputClient::SetAutocorrectRange(
    const gfx::Range& range) {
  if (autocorrect_enabled_) {
    autocorrect_range_ = range;
  }
  return autocorrect_enabled_;
}

std::optional<GrammarFragment>
DummyTextInputClient::GetGrammarFragmentAtCursor() const {
  for (const auto& fragment : grammar_fragments_) {
    if (fragment.range.Contains(cursor_range_)) {
      return fragment;
    }
  }
  return std::nullopt;
}

bool DummyTextInputClient::ClearGrammarFragments(const gfx::Range& range) {
  grammar_fragments_.clear();
  return true;
}

bool DummyTextInputClient::AddGrammarFragments(
    const std::vector<GrammarFragment>& fragments) {
  grammar_fragments_.insert(grammar_fragments_.end(), fragments.begin(),
                            fragments.end());
  return true;
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
void DummyTextInputClient::GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {}
#endif

#if BUILDFLAG(IS_WIN)
void DummyTextInputClient::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {}
#endif

}  // namespace ui
