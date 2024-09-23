// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textarea/textarea.h"

#include "base/logging.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

namespace views {

Textarea::Textarea() {
  set_placeholder_text_draw_flags(placeholder_text_draw_flags() |
                                  gfx::Canvas::MULTI_LINE);
  GetRenderText()->SetMultiline(true);
  GetRenderText()->SetVerticalAlignment(gfx::ALIGN_TOP);
  GetRenderText()->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
  SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA);
}

size_t Textarea::GetNumLines() {
  return GetRenderText()->GetNumLines();
}

bool Textarea::OnMouseWheel(const ui::MouseWheelEvent& event) {
  GetRenderText()->SetDisplayOffset(GetRenderText()->GetUpdatedDisplayOffset() +
                                    gfx::Vector2d(0, event.y_offset()));
  UpdateCursorViewPosition();
  UpdateCursorVisibility();
  SchedulePaint();
  return true;
}

Textfield::EditCommandResult Textarea::DoExecuteTextEditCommand(
    ui::TextEditCommand command) {
  bool rtl = GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  gfx::VisualCursorDirection begin = rtl ? gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT;
  gfx::VisualCursorDirection end = rtl ? gfx::CURSOR_LEFT : gfx::CURSOR_RIGHT;

  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
      textfield_model()->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_UP,
                                    gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_DOWN:
      textfield_model()->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_DOWN,
                                    gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_UP,
                                    gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_DOWN,
                                    gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::FIELD_BREAK, begin,
                                    kPageSelectionBehavior);
      break;
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::FIELD_BREAK, begin,
                                    kMoveParagraphSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::FIELD_BREAK, end,
                                    kPageSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      textfield_model()->MoveCursor(gfx::FIELD_BREAK, end,
                                    kMoveParagraphSelectionBehavior);
      break;
    default:
      return Textfield::DoExecuteTextEditCommand(command);
  }

  // TODO(jongkwon.lee): Return |cursor_changed| with actual value. It's okay
  // for now because |cursor_changed| is detected afterward in
  // |Textfield::ExecuteTextEditCommand|.
  return {false, false};
}

bool Textarea::PreHandleKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    DoInsertChar('\n');
    return true;
  }
  return false;
}

ui::TextEditCommand Textarea::GetCommandForKeyEvent(const ui::KeyEvent& event) {
  if (event.type() != ui::EventType::kKeyPressed || event.IsUnicodeKeyCode()) {
    return Textfield::GetCommandForKeyEvent(event);
  }

  const bool shift = event.IsShiftDown();
  switch (event.key_code()) {
    case ui::VKEY_UP:
      return shift ? ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION
                   : ui::TextEditCommand::MOVE_UP;
    case ui::VKEY_DOWN:
      return shift ? ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION
                   : ui::TextEditCommand::MOVE_DOWN;
    default:
      return Textfield::GetCommandForKeyEvent(event);
  }
}

BEGIN_METADATA(Textarea)
END_METADATA

}  // namespace views
