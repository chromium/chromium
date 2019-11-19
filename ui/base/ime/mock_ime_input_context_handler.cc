// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_ime_input_context_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/range/range.h"

namespace ui {

MockIMEInputContextHandler::MockIMEInputContextHandler()
    : commit_text_call_count_(0),
      set_selection_range_call_count_(0),
      update_preedit_text_call_count_(0),
      delete_surrounding_text_call_count_(0),
      last_sent_key_event_(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0) {}

MockIMEInputContextHandler::~MockIMEInputContextHandler() {}

void MockIMEInputContextHandler::CommitText(const std::string& text) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
}

void MockIMEInputContextHandler::UpdateCompositionText(
    const CompositionText& text,
    uint32_t cursor_pos,
    bool visible) {
  ++update_preedit_text_call_count_;
  last_update_composition_arg_.composition_text = text;
  last_update_composition_arg_.selection = gfx::Range(cursor_pos);
  last_update_composition_arg_.is_visible = visible;
}

#if defined(OS_CHROMEOS)
bool MockIMEInputContextHandler::SetCompositionRange(
    uint32_t before,
    uint32_t after,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  // TODO(shend): Make this work with before, after and different text contents.
  last_update_composition_arg_.composition_text.text =
      base::UTF8ToUTF16(last_commit_text_);
  return true;
}

bool MockIMEInputContextHandler::SetSelectionRange(uint32_t start,
                                                   uint32_t end) {
  ++set_selection_range_call_count_;
  last_update_composition_arg_.selection = gfx::Range(start, end);
  return true;
}
#endif

void MockIMEInputContextHandler::DeleteSurroundingText(int32_t offset,
                                                       uint32_t length) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_.offset = offset;
  last_delete_surrounding_text_arg_.length = length;
}

SurroundingTextInfo MockIMEInputContextHandler::GetSurroundingTextInfo() {
  return SurroundingTextInfo();
}

void MockIMEInputContextHandler::Reset() {
  commit_text_call_count_ = 0;
  set_selection_range_call_count_ = 0;
  update_preedit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  last_commit_text_.clear();
  last_sent_key_event_ = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
}

void MockIMEInputContextHandler::SendKeyEvent(KeyEvent* event) {
  last_sent_key_event_ = *event;
}

InputMethod* MockIMEInputContextHandler::GetInputMethod() {
  return nullptr;
}

void MockIMEInputContextHandler::ConfirmCompositionText(bool reset_engine,
                                                        bool keep_selection) {
  // TODO(b/134473433) Modify this function so that when keep_selection is
  // true, the selection is not changed when text committed
  if (keep_selection) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  if (!HasCompositionText())
    return;

  CommitText(
      base::UTF16ToUTF8(last_update_composition_arg_.composition_text.text));
  last_update_composition_arg_.composition_text.text = base::string16();
}

bool MockIMEInputContextHandler::HasCompositionText() {
  return !last_update_composition_arg_.composition_text.text.empty();
}

}  // namespace ui
