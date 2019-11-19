// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V5_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V5_H_

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

class XDGPopupWrapperV5 : public XDGPopupWrapper {
 public:
  XDGPopupWrapperV5(WaylandWindow* wayland_window);
  ~XDGPopupWrapperV5() override;

  bool Initialize(WaylandConnection* connection,
                  wl_surface* surface,
                  WaylandWindow* parent_window,
                  const gfx::Rect& bounds) override;

  // xdg_popup_listener
  static void PopupDone(void* data, xdg_popup* obj);

 private:
  WaylandWindow* wayland_window_ = nullptr;
  wl_surface* surface_ = nullptr;
  wl::Object<xdg_popup> xdg_popup_;

  DISALLOW_COPY_AND_ASSIGN(XDGPopupWrapperV5);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_V5_H_
