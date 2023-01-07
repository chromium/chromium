// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_TYPE_H_
#define UI_BASE_IME_TEXT_INPUT_TYPE_H_

namespace ui {

// TextInputType is the enum type representing every type of text input fields.
// TextInputType should include all types defined in blink::WebTextInputType
// defined in: third_party/WebKit/public/platform/WebTextInputType.h
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base.ime
enum TextInputType {
  // Input caret is not in an editable node, no input method shall be used.
  TEXT_INPUT_TYPE_NONE,

  // Input caret is in a normal editable node, any input method can be used.
  TEXT_INPUT_TYPE_TEXT,

  // Input caret is in a password box, an input method may be used only if
  // it's suitable for password input.
  TEXT_INPUT_TYPE_PASSWORD,

  TEXT_INPUT_TYPE_SEARCH,
  TEXT_INPUT_TYPE_EMAIL,
  TEXT_INPUT_TYPE_NUMBER,
  TEXT_INPUT_TYPE_TELEPHONE,
  TEXT_INPUT_TYPE_URL,
  TEXT_INPUT_TYPE_DATE,
  TEXT_INPUT_TYPE_DATE_TIME,
  TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
  TEXT_INPUT_TYPE_MONTH,
  TEXT_INPUT_TYPE_TIME,
  TEXT_INPUT_TYPE_WEEK,
  TEXT_INPUT_TYPE_TEXT_AREA,

  // Input caret is in a contenteditable node (not an INPUT field).
  TEXT_INPUT_TYPE_CONTENT_EDITABLE,

  // The focused node is date time field. The date time field does not have
  // input caret but it is necessary to distinguish from TEXT_INPUT_TYPE_NONE
  // for on-screen keyboard.
  TEXT_INPUT_TYPE_DATE_TIME_FIELD,

  // Input caret is in an editable node which doesn't support rich editing.
  // It means that the editable node cannot support the features like candidate
  // texts and retrieving text around cursor.
  // However, it still can process raw key events and needs the on-screen
  // keyboard if it wants.
  TEXT_INPUT_TYPE_NULL,

  TEXT_INPUT_TYPE_MAX = TEXT_INPUT_TYPE_NULL,
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_TYPE_H_
