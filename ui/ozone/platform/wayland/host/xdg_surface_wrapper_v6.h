// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V6_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V6_H_

#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper.h"

#include "base/macros.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

class XDGSurfaceWrapperV6 : public XDGSurfaceWrapper {
 public:
  XDGSurfaceWrapperV6(WaylandWindow* wayland_window);
  ~XDGSurfaceWrapperV6() override;

  // XDGSurfaceWrapper overrides:
  bool Initialize(WaylandConnection* connection,
                  wl_surface* surface,
                  bool with_toplevel) override;
  void SetMaximized() override;
  void UnSetMaximized() override;
  void SetFullscreen() override;
  void UnSetFullscreen() override;
  void SetMinimized() override;
  void SurfaceMove(WaylandConnection* connection) override;
  void SurfaceResize(WaylandConnection* connection, uint32_t hittest) override;
  void SetTitle(const base::string16& title) override;
  void AckConfigure() override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;
  void SetAppId(const std::string& app_id) override;

  // xdg_surface_listener
  static void Configure(void* data,
                        struct zxdg_surface_v6* zxdg_surface_v6,
                        uint32_t serial);
  static void ConfigureTopLevel(void* data,
                                struct zxdg_toplevel_v6* zxdg_toplevel_v6,
                                int32_t width,
                                int32_t height,
                                struct wl_array* states);

  // xdg_toplevel_listener
  static void CloseTopLevel(void* data,
                            struct zxdg_toplevel_v6* zxdg_toplevel_v6);

  zxdg_surface_v6* xdg_surface() const;

 private:
  WaylandWindow* wayland_window_;
  uint32_t pending_configure_serial_;
  wl::Object<zxdg_surface_v6> zxdg_surface_v6_;
  wl::Object<zxdg_toplevel_v6> zxdg_toplevel_v6_;

  bool surface_for_popup_ = false;

  DISALLOW_COPY_AND_ASSIGN(XDGSurfaceWrapperV6);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V6_H_
