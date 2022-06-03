// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

#include <linux/input.h>


namespace ui {

MouseButtonMapEvdev::MouseButtonMapEvdev() {
}

MouseButtonMapEvdev::~MouseButtonMapEvdev() {
}

void MouseButtonMapEvdev::SetPrimaryButtonRight(bool primary_button_right) {
  primary_button_right_ = primary_button_right;
}

int MouseButtonMapEvdev::GetMappedButton(uint16_t button) const {
  if (!primary_button_right_)
    return button;
  if (button == BTN_LEFT)
    return BTN_RIGHT;
  if (button == BTN_RIGHT)
    return BTN_LEFT;
  return button;
}

}  // namespace ui
