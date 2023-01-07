// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_
#define UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_

#include "ui/views/controls/textfield/textfield.h"

namespace views {

// A multiline textfield implementation.
class VIEWS_EXPORT Textarea : public Textfield {
 public:
  METADATA_HEADER(Textarea);

  Textarea();
  ~Textarea() override = default;

  // Returns the number of lines of the text.
  size_t GetNumLines();

  // Textfield:
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

 protected:
  // Textfield:
  Textfield::EditCommandResult DoExecuteTextEditCommand(
      ui::TextEditCommand command) override;
  bool PreHandleKeyPressed(const ui::KeyEvent& event) override;
  ui::TextEditCommand GetCommandForKeyEvent(const ui::KeyEvent& event) override;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_
