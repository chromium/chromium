// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V5_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V5_H_

#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper.h"

#include "base/macros.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

class XDGSurfaceWrapperV5 : public XDGSurfaceWrapper {
 public:
  XDGSurfaceWrapperV5(WaylandWindow* wayland_window);
  ~XDGSurfaceWrapperV5() override;

  // XDGSurfaceWrapper:
  // |with_toplevel| is not used with xdg_surface_v5 as long as it can have
  // only xdg_surface role.
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
                        xdg_surface* obj,
                        int32_t width,
                        int32_t height,
                        wl_array* states,
                        uint32_t serial);
  static void Close(void* data, xdg_surface* obj);

 private:
  WaylandWindow* wayland_window_;
  uint32_t pending_configure_serial_;
  wl::Object<xdg_surface> xdg_surface_;

  DISALLOW_COPY_AND_ASSIGN(XDGSurfaceWrapperV5);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_V5_H_
