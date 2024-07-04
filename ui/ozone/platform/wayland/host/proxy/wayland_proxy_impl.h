// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {
class WaylandConnection;
class WaylandShmBuffer;
class WaylandWindow;
}  // namespace ui

namespace wl {

class WaylandProxyImpl : public WaylandProxy, public ui::WaylandWindowObserver {
 public:
  explicit WaylandProxyImpl(ui::WaylandConnection* connection);
  ~WaylandProxyImpl() override;

  // WaylandProxy overrides:
  void SetDelegate(WaylandProxy::Delegate* delegate) override;
  struct wl_registry* GetRegistry() override;
  void RoundTripQueue() override;
  wl_surface* GetWlSurfaceForAcceleratedWidget(
      gfx::AcceleratedWidget widget) override;
  ui::WaylandWindow* GetWaylandWindowForAcceleratedWidget(
      gfx::AcceleratedWidget widget) override;
  wl_buffer* CreateShmBasedWlBuffer(const gfx::Size& buffer_size) override;
  void DestroyShmForWlBuffer(wl_buffer* buffer) override;
  void FlushForTesting() override;
  ui::PlatformWindowType GetWindowType(gfx::AcceleratedWidget widget) override;
  bool WindowHasPointerFocus(gfx::AcceleratedWidget widget) override;
  bool WindowHasKeyboardFocus(gfx::AcceleratedWidget widget) override;

 private:
  // ui::WaylandWindowObserver overrides:
  void OnWindowAdded(ui::WaylandWindow* window) override;
  void OnWindowRemoved(ui::WaylandWindow* window) override;
  void OnWindowConfigured(ui::WaylandWindow* window) override;
  void OnWindowRoleAssigned(ui::WaylandWindow* window) override;

  const raw_ptr<ui::WaylandConnection> connection_;

  raw_ptr<WaylandProxy::Delegate> delegate_ = nullptr;

  std::vector<ui::WaylandShmBuffer> shm_buffers_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_
