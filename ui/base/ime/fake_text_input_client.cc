// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fake_text_input_client.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

FakeTextInputClient::FakeTextInputClient(TextInputType text_input_type)
    : text_input_type_(text_input_type) {}

FakeTextInputClient::FakeTextInputClient(Options options)
    : FakeTextInputClient(/*input_method=*/nullptr, std::move(options)) {}

FakeTextInputClient::FakeTextInputClient(InputMethod* input_method,
                                         Options options)
    : input_method_(input_method),
      text_input_type_(options.type),
      mode_(options.mode),
      flags_(options.flags),
      can_insert_image_(options.can_insert_image),
      caret_bounds_(options.caret_bounds),
      should_do_learning_(options.should_do_learning) {}

FakeTextInputClient::~FakeTextInputClient() {
  Blur();
}

void FakeTextInputClient::set_text_input_type(TextInputType text_input_type) {
  text_input_type_ = text_input_type;
}

void FakeTextInputClient::set_source_id(ukm::SourceId source_id) {
  source_id_ = source_id;
}

void FakeTextInputClient::SetTextAndSelection(const std::u16string& text,
                                              gfx::Range selection) {
  DCHECK_LE(selection_.end(), text.length());
  text_ = text;
  selection_ = selection;
}

void FakeTextInputClient::Focus() {
  if (input_method_ != nullptr) {
    input_method_->SetFocusedTextInputClient(this);
  }
}

void FakeTextInputClient::Blur() {
  if (input_method_ != nullptr) {
    input_method_->SetFocusedTextInputClient(nullptr);
  }
}

base::WeakPtr<ui::TextInputClient> FakeTextInputClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeTextInputClient::SetCompositionText(
    const CompositionText& composition) {
  text_.insert(selection_.start(), composition.text);
  const size_t new_cursor = selection_.start() + composition.text.length();
  composition_range_ = gfx::Range(selection_.start(), new_cursor);
  selection_ = gfx::Range(new_cursor, new_cursor);
}

size_t FakeTextInputClient::ConfirmCompositionText(bool keep_selection) {
  return std::numeric_limits<size_t>::max();
}

void FakeTextInputClient::ClearCompositionText() {}

void FakeTextInputClient::InsertText(
    const std::u16string& text,
    TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  const gfx::Range replacement_range =
      composition_range_.is_empty() ? selection_ : composition_range_;

  text_.replace(replacement_range.start(), replacement_range.length(), text);

  if (cursor_behavior ==
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText) {
    selection_ = gfx::Range(replacement_range.start() + text.length(),
                            replacement_range.start() + text.length());
  }

  composition_range_ = gfx::Range();
}

void FakeTextInputClient::InsertChar(const KeyEvent& event) {}

bool FakeTextInputClient::CanInsertImage() {
  return can_insert_image_;
}

void FakeTextInputClient::InsertImage(const GURL& src) {
  last_inserted_image_url_ = src;
}

TextInputType FakeTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

TextInputMode FakeTextInputClient::GetTextInputMode() const {
  return mode_;
}

base::i18n::TextDirection FakeTextInputClient::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

int FakeTextInputClient::GetTextInputFlags() const {
  return flags_;
}

void FakeTextInputClient::SetFlags(const int flags) {
  flags_ = flags;
}

void FakeTextInputClient::SetUrl(const GURL& url) {
  url_ = url;
}

bool FakeTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect FakeTextInputClient::GetCaretBounds() const {
  return caret_bounds_;
}

gfx::Rect FakeTextInputClient::GetSelectionBoundingBox() const {
  return {};
}

bool FakeTextInputClient::GetCompositionCharacterBounds(size_t index,
                                                        gfx::Rect* rect) const {
  return false;
}

bool FakeTextInputClient::HasCompositionText() const {
  return !composition_range_.is_empty();
}

ui::TextInputClient::FocusReason FakeTextInputClient::GetFocusReason() const {
  return ui::TextInputClient::FOCUS_REASON_MOUSE;
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

#if BUILDFLAG(IS_MAC)
bool FakeTextInputClient::DeleteRange(const gfx::Range& range) {
  return false;
}
#endif

bool FakeTextInputClient::GetTextFromRange(const gfx::Range& range,
                                           std::u16string* text) const {
  if (range.GetMax() <= text_.length()) {
    *text = text_.substr(range.GetMin(), range.length());
    return true;
  }
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
  return source_id_;
}

bool FakeTextInputClient::ShouldDoLearning() {
  return should_do_learning_;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_CHROMEOS)
gfx::Range FakeTextInputClient::GetAutocorrectRange() const {
  return autocorrect_range_;
}

gfx::Rect FakeTextInputClient::GetAutocorrectCharacterBounds() const {
  return {};
}

bool FakeTextInputClient::SetAutocorrectRange(const gfx::Range& range) {
  autocorrect_range_ = range;
  return true;
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
void FakeTextInputClient::GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {}
#endif

#if BUILDFLAG(IS_WIN)
void FakeTextInputClient::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
ui::TextInputClient::EditingContext
FakeTextInputClient::GetTextEditingContext() {
  return EditingContext{.page_url = url_};
}
#endif

}  // namespace ui
