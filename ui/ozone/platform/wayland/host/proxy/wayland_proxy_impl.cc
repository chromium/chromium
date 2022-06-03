// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy_impl.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace wl {

WaylandProxyImpl::WaylandProxyImpl(ui::WaylandConnection* connection)
    : connection_(connection) {
  WaylandProxy::SetInstance(this);
}

WaylandProxyImpl::~WaylandProxyImpl() {
  WaylandProxy::SetInstance(nullptr);
  if (delegate_)
    connection_->wayland_window_manager()->RemoveObserver(this);
}

void WaylandProxyImpl::SetDelegate(WaylandProxy::Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  if (delegate_)
    connection_->wayland_window_manager()->AddObserver(this);
  else if (!delegate_)
    connection_->wayland_window_manager()->RemoveObserver(this);
}

wl_display* WaylandProxyImpl::GetDisplay() {
  return connection_->display();
}

wl_display* WaylandProxyImpl::GetDisplayWrapper() {
  return connection_->display_wrapper();
}

void WaylandProxyImpl::RoundTripQueue() {
  connection_->RoundTripQueue();
}

wl_surface* WaylandProxyImpl::GetWlSurfaceForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->root_surface()->surface();
}

wl_buffer* WaylandProxyImpl::CreateShmBasedWlBuffer(
    const gfx::Size& buffer_size) {
  ui::WaylandShmBuffer shm_buffer(connection_->shm(), buffer_size);
  auto* wlbuffer = shm_buffer.get();
  DCHECK(wlbuffer);
  shm_buffers_.emplace_back(std::move(shm_buffer));
  return wlbuffer;
}

void WaylandProxyImpl::DestroyShmForWlBuffer(wl_buffer* buffer) {
  auto it =
      std::find_if(shm_buffers_.begin(), shm_buffers_.end(),
                   [buffer](const auto& buf) { return buf.get() == buffer; });
  DCHECK(it != shm_buffers_.end());
  shm_buffers_.erase(it);
}

void WaylandProxyImpl::ScheduleDisplayFlush() {
  connection_->ScheduleFlush();
}

ui::PlatformWindowType WaylandProxyImpl::GetWindowType(
    gfx::AcceleratedWidget widget) {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->type();
}

gfx::Rect WaylandProxyImpl::GetWindowBounds(gfx::AcceleratedWidget widget) {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->GetBounds();
}

bool WaylandProxyImpl::WindowHasPointerFocus(gfx::AcceleratedWidget widget) {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->has_pointer_focus();
}

bool WaylandProxyImpl::WindowHasKeyboardFocus(gfx::AcceleratedWidget widget) {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->has_keyboard_focus();
}

void WaylandProxyImpl::OnWindowAdded(ui::WaylandWindow* window) {
  DCHECK(delegate_);
  delegate_->OnWindowAdded(window->GetWidget());
}

void WaylandProxyImpl::OnWindowRemoved(ui::WaylandWindow* window) {
  DCHECK(delegate_);
  delegate_->OnWindowRemoved(window->GetWidget());
}

void WaylandProxyImpl::OnWindowConfigured(ui::WaylandWindow* window) {
  DCHECK(delegate_);
  delegate_->OnWindowConfigured(window->GetWidget(),
                                window->IsSurfaceConfigured());
}

}  // namespace wl
