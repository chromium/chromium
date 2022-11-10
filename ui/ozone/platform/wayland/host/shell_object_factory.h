// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_

#include <memory>

namespace ui {

class ShellToplevelWrapper;
struct ShellPopupParams;
class ShellPopupWrapper;
class WaylandConnection;
class WaylandWindow;

// Creates Wayland shell objects.
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
      const ShellPopupParams& params);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_OBJECT_FACTORY_H_
