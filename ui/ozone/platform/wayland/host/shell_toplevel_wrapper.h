// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_

#include "base/strings/string16.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;

// A wrapper around different versions of xdg toplevels. Allows
// WaylandToplevelWindow to set window-like properties such as maximize,
// fullscreen, and minimize, set application-specific metadata like title and
// id, as well as trigger user interactive operations such as interactive resize
// and move.
class ShellToplevelWrapper {
 public:
  enum class DecorationMode {
    // Client-side decoration for a window.
    // In this case, the client is responsible for drawing decorations
    // for a window (e.g. caption bar, close button). This is suitable for
    // windows using custom frame.
    kClientSide = 1,
    // Server-side decoration for a window.
    // In this case, the ash window manager is responsible for drawing
    // decorations. This is suitable for windows using native frame.
    // e.g. taskmanager.
    kServerSide
  };

  virtual ~ShellToplevelWrapper() = default;

  // Initializes the ShellToplevel.
  virtual bool Initialize() = 0;

  // Sets a native window to maximized state.
  virtual void SetMaximized() = 0;

  // Unsets a native window from maximized state.
  virtual void UnSetMaximized() = 0;

  // Sets a native window to fullscreen state.
  virtual void SetFullscreen() = 0;

  // Unsets a native window from fullscreen state.
  virtual void UnSetFullscreen() = 0;

  // Sets a native window to minimized state.
  virtual void SetMinimized() = 0;

  // Tells wayland to start interactive window drag.
  virtual void SurfaceMove(WaylandConnection* connection) = 0;

  // Tells wayland to start interactive window resize.
  virtual void SurfaceResize(WaylandConnection* connection,
                             uint32_t hittest) = 0;

  // Sets a title of a native window.
  virtual void SetTitle(const base::string16& title) = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure(uint32_t serial) = 0;

  // Sets a desired window geometry once wayland requests client to do so.
  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;

  // Sets the minimum size for the top level.
  virtual void SetMinSize(int32_t width, int32_t height) = 0;

  // Sets the maximum size for the top level.
  virtual void SetMaxSize(int32_t width, int32_t height) = 0;

  // Sets an app id of the native window that is shown as an application name
  // and hints the compositor that it can group application surfaces together by
  // their app id. This also helps the compositor to identify application's
  // .desktop file and use the icon set there.
  virtual void SetAppId(const std::string& app_id) = 0;

  // In case of kClientSide or kServerSide, this function sends a request to the
  // wayland compositor to update the decoration mode for a surface associated
  // with this top level window.
  virtual void SetDecoration(DecorationMode decoration) = 0;
};

// Look for |value| in |wl_array| in C++ style.
bool CheckIfWlArrayHasValue(struct wl_array* wl_array, uint32_t value);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
