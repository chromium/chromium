// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"

namespace views {

BaseScrollBarButton::BaseScrollBarButton(ButtonListener* listener)
    : Button(listener),
      repeater_(base::BindRepeating(&BaseScrollBarButton::RepeaterNotifyClick,
                                    base::Unretained(this))) {}

BaseScrollBarButton::~BaseScrollBarButton() = default;

bool BaseScrollBarButton::OnMousePressed(const ui::MouseEvent& event) {
  Button::NotifyClick(event);
  repeater_.Start();
  return true;
}

void BaseScrollBarButton::OnMouseReleased(const ui::MouseEvent& event) {
  OnMouseCaptureLost();
}

void BaseScrollBarButton::OnMouseCaptureLost() {
  repeater_.Stop();
}

void BaseScrollBarButton::RepeaterNotifyClick() {
  // TODO(sky): See if we can convert to using |Screen| everywhere.
  gfx::Point cursor_point =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  ui::MouseEvent event(ui::ET_MOUSE_RELEASED, cursor_point, cursor_point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  Button::NotifyClick(event);
}

BEGIN_METADATA(BaseScrollBarButton)
METADATA_PARENT_CLASS(Button)
END_METADATA()

}  // namespace views
