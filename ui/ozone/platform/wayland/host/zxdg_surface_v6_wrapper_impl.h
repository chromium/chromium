// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_SURFACE_V6_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_SURFACE_V6_WRAPPER_IMPL_H_

#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"

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

// Surface wrapper for xdg-shell-unstable-v6
class ZXDGSurfaceV6WrapperImpl : public ShellSurfaceWrapper {
 public:
  ZXDGSurfaceV6WrapperImpl(WaylandWindow* wayland_window,
                           WaylandConnection* connection);
  ZXDGSurfaceV6WrapperImpl(const ZXDGSurfaceV6WrapperImpl&) = delete;
  ZXDGSurfaceV6WrapperImpl& operator=(const ZXDGSurfaceV6WrapperImpl&) = delete;
  ~ZXDGSurfaceV6WrapperImpl() override;

  // ShellSurfaceWrapper overrides:
  bool Initialize() override;
  void AckConfigure(uint32_t serial) override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;

  // zxdg_surface_v6_listener
  static void Configure(void* data,
                        struct zxdg_surface_v6* zxdg_surface_v6,
                        uint32_t serial);

  zxdg_surface_v6* zxdg_surface() const;

 private:
  // Non-owing WaylandWindow that uses this surface wrapper.
  WaylandWindow* const wayland_window_;
  WaylandConnection* const connection_;

  wl::Object<zxdg_surface_v6> zxdg_surface_v6_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZXDG_SURFACE_V6_WRAPPER_IMPL_H_
