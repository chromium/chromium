// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/wayland_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WaylandExtension*)
DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WaylandToplevelExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WaylandExtension*, kWaylandExtensionKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(WaylandToplevelExtension*,
                             kWaylandToplevelExtensionKey,
                             nullptr)

WaylandExtension::~WaylandExtension() = default;

void WaylandExtension::SetWaylandExtension(PlatformWindow* window,
                                           WaylandExtension* extension) {
  window->SetProperty(kWaylandExtensionKey, extension);
}

WaylandExtension* GetWaylandExtension(const PlatformWindow& window) {
  return window.GetProperty(kWaylandExtensionKey);
}

WaylandToplevelExtension::~WaylandToplevelExtension() = default;

void WaylandToplevelExtension::SetWaylandToplevelExtension(
    PlatformWindow* window,
    WaylandToplevelExtension* extension) {
  window->SetProperty(kWaylandToplevelExtensionKey, extension);
}

WaylandToplevelExtension* GetWaylandToplevelExtension(
    const PlatformWindow& window) {
  return window.GetProperty(kWaylandToplevelExtensionKey);
}

}  // namespace ui
