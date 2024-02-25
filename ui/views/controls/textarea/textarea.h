// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_
#define UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

// A multiline textfield implementation.
class VIEWS_EXPORT Textarea : public Textfield {
  METADATA_HEADER(Textarea, Textfield)

 public:
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

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Textarea, Textfield)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Textarea)

#endif  // UI_VIEWS_CONTROLS_TEXTAREA_TEXTAREA_H_
