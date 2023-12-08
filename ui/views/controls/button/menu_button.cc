// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include <memory>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/view_class_properties.h"

namespace views {

MenuButton::MenuButton(PressedCallback callback,
                       const std::u16string& text,
                       int button_context)
    : LabelButton(PressedCallback(), text, button_context) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  std::unique_ptr<MenuButtonController> menu_button_controller =
      std::make_unique<MenuButtonController>(
          this, std::move(callback),
          std::make_unique<Button::DefaultButtonControllerDelegate>(this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

MenuButton::~MenuButton() = default;

bool MenuButton::Activate(const ui::Event* event) {
  return button_controller()->Activate(event);
}

void MenuButton::SetCallback(PressedCallback callback) {
  menu_button_controller_->SetCallback(std::move(callback));
}

void MenuButton::NotifyClick(const ui::Event& event) {
  // Run pressed callback via MenuButtonController, instead of directly.
  button_controller()->Activate(&event);
}

BEGIN_METADATA(MenuButton)
END_METADATA

}  // namespace views
