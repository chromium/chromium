// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_

#include <memory>

namespace gfx {
class Rect;
}

namespace ui {

class ShellToplevelWrapper;
class ShellPopupWrapper;
class WaylandConnection;
class WaylandWindow;

// Shell factory that creates shell objects for different protocols. Preferred
// protocols are defined in the following order:
// 1) xdg-shell-stable-protocol.
// 2) xdg-shell-v6-unstable-protocol.
class ShellObjectFactory {
 public:
  ShellObjectFactory();
  ~ShellObjectFactory();

  // Creates and initializes a ShellToplevelWrapper.
  std::unique_ptr<ShellToplevelWrapper> CreateShellToplevelWrapper(
      WaylandConnection* connection,
      WaylandWindow* wayland_window);

  // Creates and intitializes a ShellPopupSurface.
  std::unique_ptr<ShellPopupWrapper> CreateShellPopupWrapper(
      WaylandConnection* connection,
      WaylandWindow* wayland_window,
      const gfx::Rect& bounds);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_
