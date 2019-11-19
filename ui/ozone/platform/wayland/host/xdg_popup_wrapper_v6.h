// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V6_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V6_H_

#include <memory>

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper.h"

namespace ui {

class XDGSurfaceWrapper;
class WaylandConnection;
class WaylandWindow;

class XDGPopupWrapperV6 : public XDGPopupWrapper {
 public:
  XDGPopupWrapperV6(std::unique_ptr<XDGSurfaceWrapper> surface,
                    WaylandWindow* wayland_window);
  ~XDGPopupWrapperV6() override;

  // XDGPopupWrapper:
  bool Initialize(WaylandConnection* connection,
                  wl_surface* surface,
                  WaylandWindow* parent_window,
                  const gfx::Rect& bounds) override;

  zxdg_positioner_v6* CreatePositioner(WaylandConnection* connection,
                                       WaylandWindow* parent_window,
                                       const gfx::Rect& bounds);

  // xdg_popup_listener
  static void Configure(void* data,
                        struct zxdg_popup_v6* zxdg_popup_v6,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height);
  static void PopupDone(void* data, struct zxdg_popup_v6* zxdg_popup_v6);

  XDGSurfaceWrapper* xdg_surface();

 private:
  WaylandWindow* const wayland_window_;
  std::unique_ptr<XDGSurfaceWrapper> zxdg_surface_v6_;
  wl::Object<zxdg_popup_v6> xdg_popup_;

  DISALLOW_COPY_AND_ASSIGN(XDGPopupWrapperV6);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V6_H_
