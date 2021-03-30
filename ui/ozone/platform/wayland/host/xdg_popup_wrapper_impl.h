// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

namespace ui {

class XDGSurfaceWrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Popup wrapper for xdg-shell stable
class XDGPopupWrapperImpl : public ShellPopupWrapper {
 public:
  XDGPopupWrapperImpl(std::unique_ptr<XDGSurfaceWrapperImpl> surface,
                      WaylandWindow* wayland_window);
  ~XDGPopupWrapperImpl() override;

  // XDGPopupWrapper:
  bool Initialize(WaylandConnection* connection,
                  const gfx::Rect& bounds) override;
  void AckConfigure(uint32_t serial) override;

 private:
  bool InitializeStable(WaylandConnection* connection,
                        const gfx::Rect& bounds,
                        XDGSurfaceWrapperImpl* parent_xdg_surface_wrapper);
  struct xdg_positioner* CreatePositioner(WaylandConnection* connection,
                                          WaylandWindow* parent_window,
                                          const gfx::Rect& bounds);

  // xdg_popup_listener
  static void Configure(void* data,
                              struct xdg_popup* xdg_popup,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height);
  static void PopupDone(void* data, struct xdg_popup* xdg_popup);

  XDGSurfaceWrapperImpl* xdg_surface_wrapper() const;

  // Non-owned WaylandWindow that uses this popup.
  WaylandWindow* const wayland_window_;

  // Ground surface for this popup.
  std::unique_ptr<XDGSurfaceWrapperImpl> xdg_surface_wrapper_;

  // XDG Shell Stable object.
  wl::Object<xdg_popup> xdg_popup_;

  DISALLOW_COPY_AND_ASSIGN(XDGPopupWrapperImpl);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
