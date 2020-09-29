// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_

#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"

#include <xdg-decoration-unstable-v1-client-protocol.h>

#include <cstdint>
#include <string>

#include "base/strings/string16.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Surface wrapper for xdg-shell stable and xdg-shell-unstable-v6
class XDGSurfaceWrapperImpl : public ShellSurfaceWrapper {
 public:
  XDGSurfaceWrapperImpl(WaylandWindow* wayland_window,
                        WaylandConnection* connection);
  XDGSurfaceWrapperImpl(const XDGSurfaceWrapperImpl&) = delete;
  XDGSurfaceWrapperImpl& operator=(const XDGSurfaceWrapperImpl&) = delete;
  ~XDGSurfaceWrapperImpl() override;

  // ShellSurfaceWrapper overrides:
  bool Initialize(bool with_toplevel) override;
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
  void SetMinSize(int32_t width, int32_t height) override;
  void SetMaxSize(int32_t width, int32_t height) override;
  void SetAppId(const std::string& app_id) override;

  // xdg_surface_listener
  static void ConfigureV6(void* data,
                          struct zxdg_surface_v6* zxdg_surface_v6,
                          uint32_t serial);
  static void ConfigureTopLevelV6(void* data,
                                  struct zxdg_toplevel_v6* zxdg_toplevel_v6,
                                  int32_t width,
                                  int32_t height,
                                  struct wl_array* states);

  static void ConfigureStable(void* data,
                              struct xdg_surface* xdg_surface,
                              uint32_t serial);
  static void ConfigureTopLevelStable(void* data,
                                      struct xdg_toplevel* xdg_toplevel,
                                      int32_t width,
                                      int32_t height,
                                      struct wl_array* states);

  // xdg_toplevel_listener
  static void CloseTopLevelStable(void* data,
                                  struct xdg_toplevel* xdg_toplevel);
  static void CloseTopLevelV6(void* data,
                              struct zxdg_toplevel_v6* zxdg_toplevel_v6);

  void SetTopLevelDecorationMode(
      zxdg_toplevel_decoration_v1_mode requested_mode);
  // zxdg_decoration_listener
  static void ConfigureDecoration(
      void* data,
      struct zxdg_toplevel_decoration_v1* decoration,
      uint32_t mode);

  struct xdg_surface* xdg_surface() const;
  zxdg_surface_v6* zxdg_surface() const;

 private:
  // Initializes using XDG Shell Stable protocol.
  bool InitializeStable(bool with_toplevel);
  // Initializes using XDG Shell V6 protocol.
  bool InitializeV6(bool with_toplevel);

  // Initializes the xdg-decoration protocol extension, if available.
  void InitializeXdgDecoration();

  // Non-owing WaylandWindow that uses this surface wrapper.
  WaylandWindow* const wayland_window_;
  WaylandConnection* const connection_;

  uint32_t pending_configure_serial_ = 0;

  wl::Object<zxdg_surface_v6> zxdg_surface_v6_;
  wl::Object<zxdg_toplevel_v6> zxdg_toplevel_v6_;
  wl::Object<struct xdg_surface> xdg_surface_;
  wl::Object<xdg_toplevel> xdg_toplevel_;
  wl::Object<zxdg_toplevel_decoration_v1> zxdg_toplevel_decoration_;

  bool surface_for_popup_ = false;

  // Keeps track of the decoration mode currently in use if xdg-decoration
  // protocol extension is available, otherwise CLIENT_SIDE is assumed.
  enum zxdg_toplevel_decoration_v1_mode decoration_mode_ =
      ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_
