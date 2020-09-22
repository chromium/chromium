// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandSurface::WaylandSurface(WaylandConnection* connection,
                               WaylandWindow* root_window)
    : connection_(connection),
      root_window_(root_window),
      surface_(connection->CreateSurface()) {}

WaylandSurface::~WaylandSurface() = default;

uint32_t WaylandSurface::GetSurfaceId() const {
  if (!surface_)
    return 0u;
  return surface_.id();
}

gfx::AcceleratedWidget WaylandSurface::GetWidget() const {
  return root_window_ ? root_window_->GetWidget() : gfx::kNullAcceleratedWidget;
}

bool WaylandSurface::Initialize() {
  if (!surface_)
    return false;

  static struct wl_surface_listener surface_listener = {
      &WaylandSurface::Enter,
      &WaylandSurface::Leave,
  };
  wl_surface_add_listener(surface_.get(), &surface_listener, this);

  return true;
}

void WaylandSurface::UnsetRootWindow() {
  DCHECK(surface_);
  root_window_ = nullptr;
}

void WaylandSurface::AttachBuffer(wl_buffer* buffer) {
  // The logic in DamageBuffer currently relies on attachment coordinates of
  // (0, 0). If this changes, then the calculation in DamageBuffer will also
  // need to be updated.
  wl_surface_attach(surface_.get(), buffer, 0, 0);
  connection_->ScheduleFlush();
}

void WaylandSurface::Damage(const gfx::Rect& pending_damage_region) {
  if (connection_->compositor_version() >=
      WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
    // wl_surface_damage_buffer relies on compositor API version 4. See
    // https://bit.ly/2u00lv6 for details.
    // We don't need to apply any scaling because pending_damage_region is
    // already in buffer coordinates.
    wl_surface_damage_buffer(
        surface_.get(), pending_damage_region.x(), pending_damage_region.y(),
        pending_damage_region.width(), pending_damage_region.height());
  } else {
    // The calculation for damage region relies on two assumptions:
    // 1) The buffer is always attached at surface location (0, 0)
    // 2) The API wl_surface::set_buffer_transform is not used.
    // It's possible to write logic that accounts for both cases above, but
    // it's currently unnecessary.
    //
    // Note: The damage region may not be an integer multiple of scale. To
    // keep the implementation simple, the x() and y() coordinates round down,
    // and the width() and height() calculations always add an extra pixel.
    wl_surface_damage(surface_.get(), pending_damage_region.x() / buffer_scale_,
                      pending_damage_region.y() / buffer_scale_,
                      pending_damage_region.width() / buffer_scale_ + 1,
                      pending_damage_region.height() / buffer_scale_ + 1);
  }
  connection_->ScheduleFlush();
}

void WaylandSurface::Commit() {
  wl_surface_commit(surface_.get());
  connection_->ScheduleFlush();
}

void WaylandSurface::SetBufferScale(int32_t new_scale, bool update_bounds) {
  DCHECK_GT(new_scale, 0);

  if (new_scale == buffer_scale_)
    return;

  buffer_scale_ = new_scale;
  wl_surface_set_buffer_scale(surface_.get(), buffer_scale_);
  connection_->ScheduleFlush();
}

void WaylandSurface::SetBounds(const gfx::Rect& bounds_px) {
  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  if (!root_window_ || !root_window_->IsOpaqueWindow())
    return;

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));
  wl_region_add(region.get(), 0, 0, bounds_px.width(), bounds_px.height());

  wl_surface_set_opaque_region(surface_.get(), region.get());

  connection_->ScheduleFlush();
}

wl::Object<wl_subsurface> WaylandSurface::CreateSubsurface(
    WaylandSurface* parent) {
  DCHECK(parent);
  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  wl::Object<wl_subsurface> subsurface(wl_subcompositor_get_subsurface(
      subcompositor, surface_.get(), parent->surface_.get()));
  return subsurface;
}

// static
void WaylandSurface::Enter(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  if (auto* root_window = static_cast<WaylandSurface*>(data)->root_window_)
    root_window->AddEnteredOutputId(output);
}

// static
void WaylandSurface::Leave(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  if (auto* root_window = static_cast<WaylandSurface*>(data)->root_window_)
    root_window->RemoveEnteredOutputId(output);
}

}  // namespace ui
