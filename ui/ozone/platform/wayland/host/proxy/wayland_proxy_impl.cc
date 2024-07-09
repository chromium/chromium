// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy_impl.h"

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace wl {

WaylandProxyImpl::WaylandProxyImpl(ui::WaylandConnection* connection)
    : connection_(connection) {
  WaylandProxy::SetInstance(this);
  connection_->window_manager()->AddObserver(this);
}

WaylandProxyImpl::~WaylandProxyImpl() {
  connection_->window_manager()->RemoveObserver(this);
  WaylandProxy::SetInstance(nullptr);
}

void WaylandProxyImpl::SetDelegate(WaylandProxy::Delegate* delegate) {
  delegate_ = delegate;
}

struct wl_registry* WaylandProxyImpl::GetRegistry() {
  return connection_->GetRegistry();
}

void WaylandProxyImpl::RoundTripQueue() {
  connection_->RoundTripQueue();
}

wl_surface* WaylandProxyImpl::GetWlSurfaceForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  auto* window = connection_->window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->root_surface()->surface();
}

ui::WaylandWindow* WaylandProxyImpl::GetWaylandWindowForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  auto* window = connection_->window_manager()->GetWindow(widget);
  DCHECK(window);
  return window;
}

wl_buffer* WaylandProxyImpl::CreateShmBasedWlBuffer(
    const gfx::Size& buffer_size) {
  ui::WaylandShmBuffer shm_buffer(connection_->buffer_factory(), buffer_size);
  auto* wlbuffer = shm_buffer.get();
  DCHECK(wlbuffer);
  shm_buffers_.emplace_back(std::move(shm_buffer));
  return wlbuffer;
}

void WaylandProxyImpl::DestroyShmForWlBuffer(wl_buffer* buffer) {
  auto it =
      base::ranges::find(shm_buffers_, buffer, &ui::WaylandShmBuffer::get);
  CHECK(it != shm_buffers_.end(), base::NotFatalUntil::M130);
  shm_buffers_.erase(it);
}

void WaylandProxyImpl::FlushForTesting() {
  connection_->Flush();
}

ui::PlatformWindowType WaylandProxyImpl::GetWindowType(
    gfx::AcceleratedWidget widget) {
  auto* window = connection_->window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->type();
}

bool WaylandProxyImpl::WindowHasPointerFocus(gfx::AcceleratedWidget widget) {
  auto* window = connection_->window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->HasPointerFocus();
}

bool WaylandProxyImpl::WindowHasKeyboardFocus(gfx::AcceleratedWidget widget) {
  auto* window = connection_->window_manager()->GetWindow(widget);
  DCHECK(window);
  return window->HasKeyboardFocus();
}

void WaylandProxyImpl::OnWindowAdded(ui::WaylandWindow* window) {
  if (delegate_) {
    delegate_->OnWindowAdded(window->GetWidget());
  }
}

void WaylandProxyImpl::OnWindowRemoved(ui::WaylandWindow* window) {
  if (delegate_) {
    delegate_->OnWindowRemoved(window->GetWidget());
  }
}

void WaylandProxyImpl::OnWindowConfigured(ui::WaylandWindow* window) {
  if (delegate_) {
    delegate_->OnWindowConfigured(window->GetWidget(),
                                  window->IsSurfaceConfigured());
  }
}

void WaylandProxyImpl::OnWindowRoleAssigned(ui::WaylandWindow* window) {
  if (delegate_) {
    delegate_->OnWindowRoleAssigned(window->GetWidget());
  }
}

}  // namespace wl
