// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_runner_views.h"

#include <stddef.h>

#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/touchui/touch_selection_menu_views.h"

namespace views {

TouchSelectionMenuRunnerViews::TestApi::TestApi(
    TouchSelectionMenuRunnerViews* menu_runner)
    : menu_runner_(menu_runner) {
  DCHECK(menu_runner_);
}

TouchSelectionMenuRunnerViews::TestApi::~TestApi() = default;

gfx::Rect TouchSelectionMenuRunnerViews::TestApi::GetAnchorRect() const {
  TouchSelectionMenuViews* menu = menu_runner_->menu_;
  return menu ? menu->GetAnchorRect() : gfx::Rect();
}

LabelButton* TouchSelectionMenuRunnerViews::TestApi::GetFirstButton() const {
  TouchSelectionMenuViews* menu = menu_runner_->menu_;
  return menu ? static_cast<LabelButton*>(menu->children().front()) : nullptr;
}

Widget* TouchSelectionMenuRunnerViews::TestApi::GetWidget() const {
  TouchSelectionMenuViews* menu = menu_runner_->menu_;
  return menu ? menu->GetWidget() : nullptr;
}
TouchSelectionMenuRunnerViews::TouchSelectionMenuRunnerViews() = default;

TouchSelectionMenuRunnerViews::~TouchSelectionMenuRunnerViews() {
  CloseMenu();
}

void TouchSelectionMenuRunnerViews::ShowMenu(
    TouchSelectionMenuViews* menu,
    const gfx::Rect& anchor_rect,
    const gfx::Size& handle_image_size) {
  menu_ = menu;
  menu_->ShowMenu(anchor_rect, handle_image_size);
}

bool TouchSelectionMenuRunnerViews::IsMenuAvailable(
    const ui::TouchSelectionMenuClient* client) const {
  return TouchSelectionMenuViews::IsMenuAvailable(client);
}

void TouchSelectionMenuRunnerViews::OpenMenu(
    ui::TouchSelectionMenuClient* client,
    const gfx::Rect& anchor_rect,
    const gfx::Size& handle_image_size,
    aura::Window* context) {
  CloseMenu();

  if (!TouchSelectionMenuViews::IsMenuAvailable(client))
    return;

  menu_ = new TouchSelectionMenuViews(this, client, context);
  menu_->ShowMenu(anchor_rect, handle_image_size);
}

void TouchSelectionMenuRunnerViews::CloseMenu() {
  if (!menu_)
    return;

  // Closing the menu sets |menu_| to nullptr and eventually deletes the object.
  menu_->CloseMenu();
  DCHECK(!menu_);
}

bool TouchSelectionMenuRunnerViews::IsRunning() const {
  return menu_ != nullptr;
}

}  // namespace views
