// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_auxiliary_window.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

// static
std::unique_ptr<WaylandWindow> WaylandWindow::Create(
    PlatformWindowDelegate* delegate,
    WaylandConnection* connection,
    PlatformWindowInitProperties properties) {
  std::unique_ptr<WaylandWindow> window;
  switch (properties.type) {
    case PlatformWindowType::kPopup:
      if (connection->IsDragInProgress()) {
        // We are in the process of drag and requested a popup. Most probably,
        // it is an arrow window.
        window = std::make_unique<WaylandAuxiliaryWindow>(delegate, connection);
        break;
      }
      FALLTHROUGH;
    case PlatformWindowType::kMenu:
      // Set the parent window in advance otherwise it is not possible to know
      // if the popup is able to find one and if WaylandWindow::Initialize()
      // fails or not. Otherwise, WaylandWindow::Create() returns nullptr and
      // makes the browser to fail. To fix this problem, search for the parent
      // window and if one is not found, create WaylandToplevelWindow instead.
      // It's also worth noting that searching twice (one time here and
      // another by WaylandPopup) is a bad practice, and the parent window is
      // set here instead. TODO(crbug.com/1078328): Feed ozone/wayland with full
      // layout info required to properly position popup windows.
      if (auto* parent_window =
              connection->wayland_window_manager()->FindParentForNewWindow(
                  properties.parent_widget)) {
        window = std::make_unique<WaylandPopup>(delegate, connection);
        window->set_parent_window(parent_window);
      } else {
        DLOG(WARNING) << "Failed to determine for menu/popup window.";
        window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      }
      break;
    case PlatformWindowType::kTooltip:
      window = std::make_unique<WaylandAuxiliaryWindow>(delegate, connection);
      break;
    case PlatformWindowType::kWindow:
    case PlatformWindowType::kBubble:
    case PlatformWindowType::kDrag:
      // TODO(msisov): Figure out what kind of surface we need to create for
      // bubble and drag windows.
      window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      break;
    default:
      NOTREACHED();
      break;
  }
  return window && window->Initialize(std::move(properties)) ? std::move(window)
                                                             : nullptr;
}

}  // namespace ui
