// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has no header guard because it is explicitly intended to be
// included more than once.

// This file also purposely has no trailing commas or leading scoping braces to
// make it easier to embed this code in C++.

TEXT_EDIT_COMMAND(DELETE_BACKWARD, kDeleteBackward)
TEXT_EDIT_COMMAND(DELETE_FORWARD, kDeleteForward)
TEXT_EDIT_COMMAND(DELETE_TO_BEGINNING_OF_LINE, kDeleteToBeginningOfLine)
TEXT_EDIT_COMMAND(DELETE_TO_BEGINNING_OF_PARAGRAPH, \
  kDeleteToBeginningOfParagraph)
TEXT_EDIT_COMMAND(DELETE_TO_END_OF_LINE, kDeleteToEndOfLine)
TEXT_EDIT_COMMAND(DELETE_TO_END_OF_PARAGRAPH, kDeleteToEndOfParagraph)
TEXT_EDIT_COMMAND(DELETE_WORD_BACKWARD, kDeleteWordBackward)
TEXT_EDIT_COMMAND(DELETE_WORD_FORWARD, kDeleteWordForward)
TEXT_EDIT_COMMAND(MOVE_BACKWARD, kMoveBackward)
TEXT_EDIT_COMMAND(MOVE_BACKWARD_AND_MODIFY_SELECTION, \
  kMoveBackwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_DOWN, kMoveDown)
TEXT_EDIT_COMMAND(MOVE_DOWN_AND_MODIFY_SELECTION, kMoveDownAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_FORWARD, kMoveForward)
TEXT_EDIT_COMMAND(MOVE_FORWARD_AND_MODIFY_SELECTION, \
  kMoveForwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_LEFT, kMoveLeft)
TEXT_EDIT_COMMAND(MOVE_LEFT_AND_MODIFY_SELECTION, kMoveLeftAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_PAGE_DOWN, kMovePageDown)
TEXT_EDIT_COMMAND(MOVE_PAGE_DOWN_AND_MODIFY_SELECTION, \
  kMovePageDownAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_PAGE_UP, kMovePageUp)
TEXT_EDIT_COMMAND(MOVE_PAGE_UP_AND_MODIFY_SELECTION, \
  kMovePageUpAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_RIGHT, kMoveRight)
TEXT_EDIT_COMMAND(MOVE_RIGHT_AND_MODIFY_SELECTION, kMoveRightAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_DOCUMENT, kMoveToBeginningOfDocument)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION, \
  kMoveToBeginningOfDocumentAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_LINE, kMoveToBeginningOfLine)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION, \
  kMoveToBeginningOfLineAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_PARAGRAPH, kMoveToBeginningOfParagraph)
TEXT_EDIT_COMMAND(MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION, \
  kMoveToBeginningOfParagraphAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_DOCUMENT, kMoveToEndOfDocument)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION, \
  kMoveToEndOfDocumentAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_LINE, kMoveToEndOfLine)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION, \
  kMoveToEndOfLineAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_PARAGRAPH, kMoveToEndOfParagraph)
TEXT_EDIT_COMMAND(MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION, \
  kMoveToEndOfParagraphAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION, \
  kMoveParagraphBackwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION, \
  kMoveParagraphForwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_UP, kMoveUp)
TEXT_EDIT_COMMAND(MOVE_UP_AND_MODIFY_SELECTION, kMoveUpAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_WORD_BACKWARD, kMoveWordBackward)
TEXT_EDIT_COMMAND(MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION, \
  kMoveWordBackwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_WORD_FORWARD, kMoveWordForward)
TEXT_EDIT_COMMAND(MOVE_WORD_FORWARD_AND_MODIFY_SELECTION, \
  kMoveWordForwardAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_WORD_LEFT, kMoveWordLeft)
TEXT_EDIT_COMMAND(MOVE_WORD_LEFT_AND_MODIFY_SELECTION, \
  kMoveWordLeftAndModifySelection)
TEXT_EDIT_COMMAND(MOVE_WORD_RIGHT, kMoveWordRight)
TEXT_EDIT_COMMAND(MOVE_WORD_RIGHT_AND_MODIFY_SELECTION, \
  kMoveWordRightAndModifySelection)
// On Mac, scroll commands have different shortcuts as move commands.
TEXT_EDIT_COMMAND(SCROLL_PAGE_DOWN, kScrollPageDown)
TEXT_EDIT_COMMAND(SCROLL_PAGE_UP, kScrollPageUp)
TEXT_EDIT_COMMAND(SCROLL_TO_BEGINNING_OF_DOCUMENT, kScrollToBeginningOfDocument)
TEXT_EDIT_COMMAND(SCROLL_TO_END_OF_DOCUMENT, kScrollToEndOfDocument)
TEXT_EDIT_COMMAND(UNDO, kUndo)
TEXT_EDIT_COMMAND(REDO, kRedo)
TEXT_EDIT_COMMAND(CUT, kCut)
TEXT_EDIT_COMMAND(COPY, kCopy)
TEXT_EDIT_COMMAND(PASTE, kPaste)
TEXT_EDIT_COMMAND(SELECT_ALL, kSelectAll)
TEXT_EDIT_COMMAND(SELECT_WORD, kSelectWord)
TEXT_EDIT_COMMAND(TRANSPOSE, kTranspose)
TEXT_EDIT_COMMAND(YANK, kYank)
TEXT_EDIT_COMMAND(INSERT_TEXT, kInsertText)
TEXT_EDIT_COMMAND(SET_MARK, kSetMark)
TEXT_EDIT_COMMAND(UNSELECT, kUnselect)
// INVALID_COMMAND must be the last one. Add new commands before it.
TEXT_EDIT_COMMAND(INVALID_COMMAND, kInvalidCommand)

// Below are commands on Mac with default system key bindings that are
// not supported yet.
//
// centerSelectionInVisibleArea                      Ctrl + L
// complete                                          Option + Esc
// deleteBackwardByDecomposingPreviousCharacter      Ctrl + Delete
