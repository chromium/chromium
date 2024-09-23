// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/mock_ime_input_context_handler.h"

#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/range/range.h"

namespace ash {

MockIMEInputContextHandler::MockIMEInputContextHandler()
    : commit_text_call_count_(0),
      update_preedit_text_call_count_(0),
      delete_surrounding_text_call_count_(0) {}

MockIMEInputContextHandler::~MockIMEInputContextHandler() = default;

void MockIMEInputContextHandler::CommitText(
    const std::u16string& text,
    ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
  observers_.Notify(&Observer::OnCommitText, text);
}

void MockIMEInputContextHandler::UpdateCompositionText(
    const ui::CompositionText& text,
    uint32_t cursor_pos,
    bool visible) {
  ++update_preedit_text_call_count_;
  last_update_composition_arg_.composition_text = text;
  last_update_composition_arg_.selection = gfx::Range(cursor_pos);
  last_update_composition_arg_.is_visible = visible;
}

bool MockIMEInputContextHandler::SetCompositionRange(
    uint32_t before,
    uint32_t after,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  // TODO(shend): Make this work with before, after and different text contents.
  last_update_composition_arg_.composition_text.text = last_commit_text_;
  return true;
}

bool MockIMEInputContextHandler::SetComposingRange(
    uint32_t start,
    uint32_t end,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  // TODO(shend): Make this work with start, end and different text contents.
  last_update_composition_arg_.composition_text.text = last_commit_text_;
  return true;
}

gfx::Range MockIMEInputContextHandler::GetAutocorrectRange() {
  return autocorrect_range_;
}

void MockIMEInputContextHandler::SetAutocorrectRange(
    const gfx::Range& range,
    SetAutocorrectRangeDoneCallback callback) {
  if (autocorrect_enabled_) {
    autocorrect_range_ = range;
  }

  std::move(callback).Run(autocorrect_enabled_);
}

std::optional<ui::GrammarFragment>
MockIMEInputContextHandler::GetGrammarFragmentAtCursor() {
  for (const auto& fragment : grammar_fragments_) {
    if (fragment.range.Contains(cursor_range_)) {
      return fragment;
    }
  }
  return std::nullopt;
}

bool MockIMEInputContextHandler::ClearGrammarFragments(
    const gfx::Range& range) {
  std::vector<ui::GrammarFragment> updated_fragments;
  for (const ui::GrammarFragment& fragment : grammar_fragments_) {
    if (!range.Contains(fragment.range)) {
      updated_fragments.push_back(fragment);
    }
  }
  grammar_fragments_ = updated_fragments;
  return true;
}

bool MockIMEInputContextHandler::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  grammar_fragments_.insert(grammar_fragments_.end(), fragments.begin(),
                            fragments.end());
  return true;
}

void MockIMEInputContextHandler::DeleteSurroundingText(
    uint32_t num_char16s_before_cursor,
    uint32_t num_char16s_after_cursor) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_.num_char16s_before_cursor =
      num_char16s_before_cursor;
  last_delete_surrounding_text_arg_.num_char16s_after_cursor =
      num_char16s_after_cursor;
}

void MockIMEInputContextHandler::ReplaceSurroundingText(
    uint32_t length_before_selection,
    uint32_t length_after_selection,
    const std::u16string_view replacement_text) {
  last_replace_surrounding_text_arg_.length_before_selection =
      length_before_selection;
  last_replace_surrounding_text_arg_.length_after_selection =
      length_after_selection;
  last_replace_surrounding_text_arg_.replacement_text =
      std::u16string(replacement_text);
}

SurroundingTextInfo MockIMEInputContextHandler::GetSurroundingTextInfo() {
  SurroundingTextInfo info;
  info.selection_range = cursor_range_;
  info.offset = 0;
  return info;
}

void MockIMEInputContextHandler::Reset() {
  commit_text_call_count_ = 0;
  update_preedit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  autocorrect_enabled_ = true;
  last_commit_text_.clear();
  sent_key_events_.clear();
}

void MockIMEInputContextHandler::SendKeyEvent(ui::KeyEvent* event) {
  sent_key_events_.emplace_back(*event);
}

ui::InputMethod* MockIMEInputContextHandler::GetInputMethod() {
  return nullptr;
}

void MockIMEInputContextHandler::ConfirmComposition(bool reset_engine) {
  // TODO(b/134473433) Modify this function so that the selection is unchanged.
  NOTIMPLEMENTED_LOG_ONCE();

  if (!HasCompositionText()) {
    return;
  }

  CommitText(
      last_update_composition_arg_.composition_text.text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  last_update_composition_arg_.composition_text.text = std::u16string();
}

bool MockIMEInputContextHandler::HasCompositionText() {
  return !last_update_composition_arg_.composition_text.text.empty();
}

ukm::SourceId MockIMEInputContextHandler::GetClientSourceForMetrics() {
  return ukm::kInvalidSourceId;
}

void MockIMEInputContextHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockIMEInputContextHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
