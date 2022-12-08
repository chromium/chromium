// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "ui/gfx/native_widget_types.h"
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
    PlatformWindowInitProperties properties,
    bool update_visual_size_immediately,
    bool apply_pending_state_on_update_visual_size) {
  std::unique_ptr<WaylandWindow> window;
  switch (properties.type) {
    case PlatformWindowType::kPopup:
    case PlatformWindowType::kTooltip:
    case PlatformWindowType::kMenu:
      // kPopup can be created by MessagePopupView without a parent window set.
      // It looks like it ought to be a global notification window. Thus, use a
      // toplevel window instead.
      if (auto* parent = connection->window_manager()->GetWindow(
              properties.parent_widget)) {
        window = std::make_unique<WaylandPopup>(delegate, connection, parent);
      } else {
        DLOG(WARNING) << "Failed to determine for menu/popup window.";
        window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      }
      break;
    case PlatformWindowType::kWindow:
    case PlatformWindowType::kBubble:
    case PlatformWindowType::kDrag:
      // TODO(crbug.com/1399419): Figure out what kind of surface we need to
      // create for kBubble and kDrag windows.
      window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      break;
    default:
      NOTREACHED();
      break;
  }
  window->set_update_visual_size_immediately_for_testing(
      update_visual_size_immediately);
  window->set_apply_pending_state_on_update_visual_size_for_testing(
      apply_pending_state_on_update_visual_size);
  return window && window->Initialize(std::move(properties)) ? std::move(window)
                                                             : nullptr;
}

}  // namespace ui
