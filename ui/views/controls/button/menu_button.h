// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

class ButtonListener;
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

  // Create a Button.
  MenuButton(const base::string16& text,
             ButtonListener* button_listener,
             int button_context = style::CONTEXT_BUTTON);
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

  DISALLOW_COPY_AND_ASSIGN(MenuButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
