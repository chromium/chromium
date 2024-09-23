// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/text_edit_command_auralinux.h"

#include "base/notreached.h"
#include "ui/base/ime/text_edit_commands.h"

namespace ui {

// This is sent to the renderer. Keep the string representation in sync with
// third_party/WebKit/public/platform/WebEditingCommandType.h.
std::string TextEditCommandAuraLinux::GetCommandString() const {
  switch (command_) {
    case TextEditCommand::DELETE_BACKWARD:
      return "DeleteBackward";
    case TextEditCommand::DELETE_FORWARD:
      return "DeleteForward";
    case TextEditCommand::DELETE_TO_BEGINNING_OF_LINE:
      return "DeleteToBeginningOfLine";
    case TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH:
      return "DeleteToBeginningOfParagraph";
    case TextEditCommand::DELETE_TO_END_OF_LINE:
      return "DeleteToEndOfLine";
    case TextEditCommand::DELETE_TO_END_OF_PARAGRAPH:
      return "DeleteToEndOfParagraph";
    case TextEditCommand::DELETE_WORD_BACKWARD:
      return "DeleteWordBackward";
    case TextEditCommand::DELETE_WORD_FORWARD:
      return "DeleteWordForward";
    case TextEditCommand::MOVE_BACKWARD:
      return "MoveBackward";
    case TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION:
      return "MoveBackwardAndModifySelection";
    case TextEditCommand::MOVE_DOWN:
      return "MoveDown";
    case TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION:
      return "MoveDownAndModifySelection";
    case TextEditCommand::MOVE_FORWARD:
      return "MoveForward";
    case TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION:
      return "MoveForwardAndModifySelection";
    case TextEditCommand::MOVE_LEFT:
      return "MoveLeft";
    case TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION:
      return "MoveLeftAndModifySelection";
    case TextEditCommand::MOVE_PAGE_DOWN:
      return "MovePageDown";
    case TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION:
      return "MovePageDownAndModifySelection";
    case TextEditCommand::MOVE_PAGE_UP:
      return "MovePageUp";
    case TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION:
      return "MovePageUpAndModifySelection";
    case TextEditCommand::MOVE_RIGHT:
      return "MoveRight";
    case TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION:
      return "MoveRightAndModifySelection";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT:
      return "MoveToBeginningOfDocument";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION:
      return "MoveToBeginningOfDocumentAndModifySelection";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_LINE:
      return "MoveToBeginningOfLine";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION:
      return "MoveToBeginningOfLineAndModifySelection";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH:
      return "MoveToBeginningOfParagraph";
    case TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      return "MoveToBeginningOfParagraphAndModifySelection";
    case TextEditCommand::MOVE_TO_END_OF_DOCUMENT:
      return "MoveToEndOfDocument";
    case TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION:
      return "MoveToEndOfDocumentAndModifySelection";
    case TextEditCommand::MOVE_TO_END_OF_LINE:
      return "MoveToEndOfLine";
    case TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION:
      return "MoveToEndOfLineAndModifySelection";
    case TextEditCommand::MOVE_TO_END_OF_PARAGRAPH:
      return "MoveToEndOfParagraph";
    case TextEditCommand::MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      return "MoveToEndOfParagraphAndModifySelection";
    case TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION:
      return "MoveParagraphBackwardAndModifySelection";
    case TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION:
      return "MoveParagraphForwardAndModifySelection";
    case TextEditCommand::MOVE_UP:
      return "MoveUp";
    case TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION:
      return "MoveUpAndModifySelection";
    case TextEditCommand::MOVE_WORD_BACKWARD:
      return "MoveWordBackward";
    case TextEditCommand::MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION:
      return "MoveWordBackwardAndModifySelection";
    case TextEditCommand::MOVE_WORD_FORWARD:
      return "MoveWordForward";
    case TextEditCommand::MOVE_WORD_FORWARD_AND_MODIFY_SELECTION:
      return "MoveWordForwardAndModifySelection";
    case TextEditCommand::MOVE_WORD_LEFT:
      return "MoveWordLeft";
    case TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION:
      return "MoveWordLeftAndModifySelection";
    case TextEditCommand::MOVE_WORD_RIGHT:
      return "MoveWordRight";
    case TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION:
      return "MoveWordRightAndModifySelection";
    case TextEditCommand::UNDO:
      return "Undo";
    case TextEditCommand::REDO:
      return "Redo";
    case TextEditCommand::CUT:
      return "Cut";
    case TextEditCommand::COPY:
      return "Copy";
    case TextEditCommand::PASTE:
      return "Paste";
    case TextEditCommand::SELECT_ALL:
      return "SelectAll";
    case TextEditCommand::SELECT_WORD:
      return "SelectWord";
    case TextEditCommand::TRANSPOSE:
      return "Transpose";
    case TextEditCommand::YANK:
      return "Yank";
    case TextEditCommand::INSERT_TEXT:
      return "InsertText";
    case TextEditCommand::SET_MARK:
      return "SetMark";
    case TextEditCommand::UNSELECT:
      return "Unselect";
    case TextEditCommand::SCROLL_PAGE_DOWN:
    case TextEditCommand::SCROLL_PAGE_UP:
    case TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT:
    case TextEditCommand::SCROLL_TO_END_OF_DOCUMENT:
      // Scroll commands are used by Mac only
    case TextEditCommand::INVALID_COMMAND:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace ui
