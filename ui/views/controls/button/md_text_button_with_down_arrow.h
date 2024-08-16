// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {

// The material design themed text button with a drop arrow displayed on the
// right side.
class VIEWS_EXPORT MdTextButtonWithDownArrow : public MdTextButton {
  METADATA_HEADER(MdTextButtonWithDownArrow, MdTextButton)

 public:
  MdTextButtonWithDownArrow(PressedCallback callback,
                            const std::u16string& text);
  MdTextButtonWithDownArrow(const MdTextButtonWithDownArrow&) = delete;
  MdTextButtonWithDownArrow& operator=(const MdTextButtonWithDownArrow&) =
      delete;
  ~MdTextButtonWithDownArrow() override;

 protected:
  // views::MdTextButton:
  void OnThemeChanged() override;
  void StateChanged(ButtonState old_state) override;

 private:
  void SetDropArrowImage();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
