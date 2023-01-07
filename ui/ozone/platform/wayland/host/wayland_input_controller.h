// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_CONTROLLER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_CONTROLLER_H_

#include "ui/ozone/public/input_controller.h"

namespace ui {

class WaylandConnection;

// Create an input controller for wayland platform.
std::unique_ptr<InputController> CreateWaylandInputController(
    WaylandConnection* connection);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_CONTROLLER_H_
