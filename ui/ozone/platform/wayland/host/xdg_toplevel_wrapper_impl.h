// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"

namespace ui {

class XDGSurfaceWrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Toplevel wrapper for xdg-shell stable
class XDGToplevelWrapperImpl : public ShellToplevelWrapper {
 public:
  XDGToplevelWrapperImpl(std::unique_ptr<XDGSurfaceWrapperImpl> surface,
                         WaylandWindow* wayland_window,
                         WaylandConnection* connection);
  XDGToplevelWrapperImpl(const XDGToplevelWrapperImpl&) = delete;
  XDGToplevelWrapperImpl& operator=(const XDGToplevelWrapperImpl&) = delete;
  ~XDGToplevelWrapperImpl() override;

  // ShellSurfaceWrapper overrides:
  bool Initialize() override;
  void SetMaximized() override;
  void UnSetMaximized() override;
  void SetFullscreen() override;
  void UnSetFullscreen() override;
  void SetMinimized() override;
  void SurfaceMove(WaylandConnection* connection) override;
  void SurfaceResize(WaylandConnection* connection, uint32_t hittest) override;
  void SetTitle(const base::string16& title) override;
  void AckConfigure(uint32_t serial) override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;
  void SetMinSize(int32_t width, int32_t height) override;
  void SetMaxSize(int32_t width, int32_t height) override;
  void SetAppId(const std::string& app_id) override;
  void SetDecoration(DecorationMode decoration) override;

  XDGSurfaceWrapperImpl* xdg_surface_wrapper() const;

 private:
  // xdg_toplevel_listener
  static void ConfigureTopLevel(void* data,
                                struct xdg_toplevel* xdg_toplevel,
                                int32_t width,
                                int32_t height,
                                struct wl_array* states);
  static void CloseTopLevel(void* data, struct xdg_toplevel* xdg_toplevel);

  // zxdg_decoration_listener
  static void ConfigureDecoration(
      void* data,
      struct zxdg_toplevel_decoration_v1* decoration,
      uint32_t mode);

  // Send request to wayland compositor to enable a requested decoration mode.
  void SetTopLevelDecorationMode(DecorationMode requested_mode);

  // Initializes the xdg-decoration protocol extension, if available.
  void InitializeXdgDecoration();

  // Ground surface for this toplevel wrapper.
  std::unique_ptr<XDGSurfaceWrapperImpl> xdg_surface_wrapper_;

  // Non-owing WaylandWindow that uses this toplevel wrapper.
  WaylandWindow* const wayland_window_;
  WaylandConnection* const connection_;

  // XDG Shell Stable object.
  wl::Object<xdg_toplevel> xdg_toplevel_;

  wl::Object<zxdg_toplevel_decoration_v1> zxdg_toplevel_decoration_;

  // On client side, it keeps track of the decoration mode currently in
  // use if xdg-decoration protocol extension is available.
  DecorationMode decoration_mode_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_
