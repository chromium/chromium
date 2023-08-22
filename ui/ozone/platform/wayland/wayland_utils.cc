// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_utils.h"

#include "ui/gfx/image/image_skia.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

namespace ui {

WaylandUtils::WaylandUtils(WaylandConnection* connection)
    : connection_(connection) {}

WaylandUtils::~WaylandUtils() = default;

gfx::ImageSkia WaylandUtils::GetNativeWindowIcon(intptr_t target_window_id) {
  return {};
}

std::string WaylandUtils::GetWmWindowClass(
    const std::string& desktop_base_name) {
  return desktop_base_name;
}

void WaylandUtils::OnUnhandledKeyEvent(const KeyEvent& key_event) {
  auto* seat = connection_->seat();
  if (!seat)
    return;
  auto* keyboard = seat->keyboard();
  if (!keyboard)
    return;
  keyboard->OnUnhandledKeyEvent(key_event);
}

}  // namespace ui
