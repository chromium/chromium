// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_menu_utils.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

namespace ui {

WaylandMenuUtils::WaylandMenuUtils(WaylandConnection* connection)
    : connection_(connection) {}

WaylandMenuUtils::~WaylandMenuUtils() = default;

int WaylandMenuUtils::GetCurrentKeyModifiers() const {
  DCHECK(connection_);
  DCHECK(connection_->event_source());
  return connection_->event_source()->keyboard_modifiers();
}

}  // namespace ui
