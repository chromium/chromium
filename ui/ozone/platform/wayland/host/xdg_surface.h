// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class XdgPopup;
class XdgToplevel;
class WaylandConnection;
class WaylandWindow;

// Wrapper class for xdg-shell's xdg_surface protocol objects. Provides
// factory methods for both xdg toplevels and popups, as well as shared
// requests and events between them.
class XdgSurface {
 public:
  XdgSurface(WaylandWindow* window, WaylandConnection* connection);
  XdgSurface(const XdgSurface&) = delete;
  XdgSurface& operator=(const XdgSurface&) = delete;
  ~XdgSurface();

  bool Initialize();
  void AckConfigure(uint32_t serial);
  bool IsConfigured();
  void SetWindowGeometry(const gfx::Rect& bounds);

 private:
  friend class XdgPopup;
  friend class XdgToplevel;

  struct xdg_surface* wl_object() const { return xdg_surface_.get(); }

  // xdg_surface_listener callbacks:
  static void OnConfigure(void* data,
                          struct xdg_surface* xdg_surface,
                          uint32_t serial);

  // Non-owing WaylandWindow that uses this surface wrapper.
  const raw_ptr<WaylandWindow> wayland_window_;
  const raw_ptr<WaylandConnection> connection_;

  bool is_configured_ = false;
  int64_t last_acked_serial_ = -1;

  wl::Object<struct xdg_surface> xdg_surface_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_H_
