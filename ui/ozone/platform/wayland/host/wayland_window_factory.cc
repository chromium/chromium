// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_bubble.h"
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
    case PlatformWindowType::kBubble:
      // kPopup can be created by MessagePopupView without a parent window set.
      // It looks like it ought to be a global notification window. Thus, use a
      // toplevel window instead.
      if (auto* parent = connection->window_manager()->GetWindow(
              properties.parent_widget)) {
        window = std::make_unique<WaylandBubble>(delegate, connection, parent);
      } else {
        // TODO(crbug.com/40883130): Make sure bubbles/popups pass a parent
        // window.
        DLOG(WARNING) << "Failed to determine parent for bubble/popup window.";
        window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      }
      break;
    case PlatformWindowType::kTooltip:
      if (auto* parent = connection->window_manager()->GetWindow(
              properties.parent_widget)) {
        WaylandWindow* root_parent = parent->GetRootParentWindow();
        if (!parent->AsWaylandPopup() &&
            root_parent->HasVisibleSecuritySurfaceBubble()) {
          // Wayland compositors always stack xdg_popups above wl_subsurfaces of
          // the parent toplevel. Back the tooltip with a sibling wl_subsurface
          // while a security-surface bubble (e.g. Autofill popup) is visible so
          // the security surface stays stacked above the tooltip. As a
          // subsurface, the tooltip loses xdg_positioner constraint adjustment
          // and may be partly off screen near the window edges, the same as
          // the security surface itself.
          window = std::make_unique<WaylandBubble>(delegate, connection,
                                                   root_parent);
        } else if (parent->AsWaylandBubble()) {
          // A bubble is a wl_subsurface, so an xdg_popup for its tooltip would
          // have to use the bubble's xdg_toplevel as its parent. xdg_positioner
          // doesn't allow the anchor rect to extend outside the parent's window
          // geometry, so the tooltip doesn't show up on some compositors when
          // the bubble extends outside the toplevel. Tooltips don't need a
          // grab, so make them subsurfaces of the bubble instead.
          window =
              std::make_unique<WaylandBubble>(delegate, connection, parent);
        } else {
          window = std::make_unique<WaylandPopup>(delegate, connection, parent);
        }
      } else {
        DLOG(WARNING) << "Failed to determine parent for tooltip window.";
        window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      }
      break;
    case PlatformWindowType::kMenu:
      if (auto* parent = connection->window_manager()->GetWindow(
              properties.parent_widget)) {
        window = std::make_unique<WaylandPopup>(delegate, connection, parent);
      } else {
        DLOG(WARNING) << "Failed to determine parent for menu window.";
        window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      }
      break;
    case PlatformWindowType::kWindow:
    case PlatformWindowType::kDrag:
      // TODO(crbug.com/40883130): Figure out what kind of surface we need to
      // create kDrag windows.
      window = std::make_unique<WaylandToplevelWindow>(delegate, connection);
      break;
    default:
      NOTREACHED();
  }
  return window && window->Initialize(std::move(properties)) ? std::move(window)
                                                             : nullptr;
}

}  // namespace ui
