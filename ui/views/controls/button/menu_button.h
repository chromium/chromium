// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

class MenuButtonController;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton
//
//  A button that shows a menu when the left mouse button is pushed
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT MenuButton : public LabelButton {
 public:
  METADATA_HEADER(MenuButton);
  explicit MenuButton(PressedCallback callback = PressedCallback(),
                      const base::string16& text = base::string16(),
                      int button_context = style::CONTEXT_BUTTON);
  MenuButton(const MenuButton&) = delete;
  MenuButton& operator=(const MenuButton&) = delete;
  ~MenuButton() override;

  MenuButtonController* button_controller() const {
    return menu_button_controller_;
  }

  bool Activate(const ui::Event* event);

 protected:
  // Button:
  void NotifyClick(const ui::Event& event) final;

 private:
  MenuButtonController* menu_button_controller_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MenuButton, LabelButton)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MenuButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
