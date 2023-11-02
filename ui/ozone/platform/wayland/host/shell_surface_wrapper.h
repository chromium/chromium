// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_SURFACE_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_SURFACE_WRAPPER_H_

#include <cstdint>

namespace gfx {
class Rect;
}

namespace ui {

// Wrapper interface for different wayland xdg-shell surface versions.
class ShellSurfaceWrapper {
 public:
  virtual ~ShellSurfaceWrapper() {}

  // Initializes the ShellSurface. The implementation should not commit surface
  // state changes and defer that to the window that owns the surface, such that
  // the window properties are set before the initial commit
  // (https://crbug.com/1268308).
  virtual bool Initialize() = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure(uint32_t serial) = 0;

  // Tells if the surface has been AckConfigured at least once.
  virtual bool IsConfigured() = 0;

  // Sets a desired window geometry once wayland requests client to do so.
  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_SURFACE_WRAPPER_H_
