// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_POPUP_V6_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_POPUP_V6_WRAPPER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

namespace ui {

class ZXDGSurfaceV6WrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Popup wrapper for xdg-shell-unstable-v6
class ZXDGPopupV6WrapperImpl : public ShellPopupWrapper {
 public:
  ZXDGPopupV6WrapperImpl(std::unique_ptr<ZXDGSurfaceV6WrapperImpl> surface,
                         WaylandWindow* wayland_window);
  ~ZXDGPopupV6WrapperImpl() override;

  // XDGPopupWrapper:
  bool Initialize(WaylandConnection* connection,
                  const gfx::Rect& bounds) override;
  void AckConfigure(uint32_t serial) override;

 private:
  bool InitializeV6(WaylandConnection* connection,
                    const gfx::Rect& bounds,
                    ZXDGSurfaceV6WrapperImpl* parent_zxdg_surface_v6_wrapper);
  struct zxdg_positioner_v6* CreatePositioner(WaylandConnection* connection,
                                              WaylandWindow* parent_window,
                                              const gfx::Rect& bounds);

  // zxdg_popup_v6_listener
  static void Configure(void* data,
                        struct zxdg_popup_v6* zxdg_popup_v6,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height);
  static void PopupDone(void* data, struct zxdg_popup_v6* zxdg_popup_v6);

  ZXDGSurfaceV6WrapperImpl* zxdg_surface_v6_wrapper() const;

  // Non-owned WaylandWindow that uses this popup.
  WaylandWindow* const wayland_window_;

  // Ground surface for this popup.
  std::unique_ptr<ZXDGSurfaceV6WrapperImpl> zxdg_surface_v6_wrapper_;

  // XDG Shell V6 object.
  wl::Object<zxdg_popup_v6> zxdg_popup_v6_;

  DISALLOW_COPY_AND_ASSIGN(ZXDGPopupV6WrapperImpl);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_POPUP_V6_WRAPPER_IMPL_H_
