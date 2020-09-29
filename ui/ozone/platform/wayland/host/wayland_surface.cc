// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <viewporter-client-protocol.h>

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
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

  if (connection_->viewporter()) {
    viewport_.reset(
        wp_viewporter_get_viewport(connection_->viewporter(), surface()));
    if (!viewport_) {
      LOG(ERROR) << "Failed to create wp_viewport";
      return false;
    }
  }

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

void WaylandSurface::UpdateBufferDamageRegion(
    const gfx::Rect& pending_damage_region,
    const gfx::Size& buffer_size) {
  // Buffer-local coordinates are in pixels, surface coordinates are in DIP.
  // The coordinate transformations from buffer pixel coordinates up to
  // the surface-local coordinates happen in the following order:
  //   1. buffer_transform (wl_surface.set_buffer_transform)
  //   2. buffer_scale (wl_surface.set_buffer_scale)
  //   3. crop and scale (wp_viewport.set*)
  // Apply buffer_transform (wl_surface.set_buffer_transform).
  gfx::Size bounds = wl::ApplyWaylandTransform(
      buffer_size, wl::ToWaylandTransform(buffer_transform_));
  // Apply buffer_scale (wl_surface.set_buffer_scale).
  bounds = gfx::ScaleToCeiledSize(bounds, 1.f / buffer_scale_);
  // Apply crop (wp_viewport.set_source).
  gfx::Rect viewport_src = gfx::Rect(bounds);
  if (!crop_rect_.IsEmpty()) {
    viewport_src = gfx::ToEnclosedRect(
        gfx::ScaleRect(crop_rect_, bounds.width(), bounds.height()));
    wp_viewport_set_source(viewport(), wl_fixed_from_int(viewport_src.x()),
                           wl_fixed_from_int(viewport_src.y()),
                           wl_fixed_from_int(viewport_src.width()),
                           wl_fixed_from_int(viewport_src.height()));
  }
  // Apply viewport scale (wp_viewport.set_destination).
  gfx::Size viewport_dst = bounds;
  if (!display_size_px_.IsEmpty()) {
    viewport_dst =
        gfx::ScaleToCeiledSize(display_size_px_, 1.f / buffer_scale_);
    wp_viewport_set_destination(viewport(), viewport_dst.width(),
                                viewport_dst.height());
  }

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
    // Calculate the damage region in surface coordinates.
    // The calculation for damage region relies on the assumption: The buffer is
    // always attached at surface location (0, 0).
    // It's possible to write logic that accounts for attaching buffer at other
    // locations, but it's currently unnecessary.

    // Apply buffer_transform (wl_surface.set_buffer_transform).
    gfx::Rect damage =
        wl::ApplyWaylandTransform(pending_damage_region, buffer_size,
                                  wl::ToWaylandTransform(buffer_transform_));
    // Apply buffer_scale (wl_surface.set_buffer_scale).
    damage = gfx::ScaleToEnclosingRect(damage, 1.f / buffer_scale_);
    // Adjust coordinates to |viewport_src| (wp_viewport.set_source).
    damage = wl::TranslateBoundsToParentCoordinates(damage, viewport_src);
    // Apply viewport scale (wp_viewport.set_destination).
    damage = gfx::ScaleToEnclosingRect(
        damage, static_cast<float>(viewport_dst.width()) / viewport_src.width(),
        static_cast<float>(viewport_dst.height()) / viewport_src.height());

    wl_surface_damage(surface_.get(), damage.x(), damage.y(), damage.width(),
                      damage.height());
  }

  connection_->ScheduleFlush();
}

void WaylandSurface::Commit() {
  wl_surface_commit(surface_.get());
  connection_->ScheduleFlush();
}

void WaylandSurface::SetBufferTransform(gfx::OverlayTransform transform) {
  DCHECK(transform != gfx::OVERLAY_TRANSFORM_INVALID);
  if (buffer_transform_ == transform)
    return;

  buffer_transform_ = transform;
  wl_output_transform wl_transform = wl::ToWaylandTransform(buffer_transform_);
  wl_surface_set_buffer_transform(surface_.get(), wl_transform);
}

void WaylandSurface::SetBufferScale(int32_t new_scale, bool update_bounds) {
  DCHECK_GT(new_scale, 0);

  if (new_scale == buffer_scale_)
    return;

  buffer_scale_ = new_scale;
  wl_surface_set_buffer_scale(surface_.get(), buffer_scale_);
  connection_->ScheduleFlush();
}

void WaylandSurface::SetOpaqueRegion(const gfx::Rect& region_px) {
  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  if (!root_window_ || !root_window_->IsOpaqueWindow())
    return;

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));
  gfx::Rect region_dip =
      gfx::ScaleToEnclosingRect(region_px, 1.f / buffer_scale_);
  wl_region_add(region.get(), region_dip.x(), region_dip.y(),
                region_dip.width(), region_dip.height());

  wl_surface_set_opaque_region(surface_.get(), region.get());

  connection_->ScheduleFlush();
}

void WaylandSurface::SetViewportSource(const gfx::RectF& src_rect) {
  if (src_rect == crop_rect_) {
    return;
  } else if (src_rect.IsEmpty() || src_rect == gfx::RectF{0.f, 0.f, 1.f, 1.f}) {
    wp_viewport_set_source(viewport(), wl_fixed_from_int(-1),
                           wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                           wl_fixed_from_int(-1));
    return;
  }

  crop_rect_ = src_rect;
}

void WaylandSurface::SetViewportDestination(const gfx::Size& dest_size_px) {
  if (dest_size_px == display_size_px_) {
    return;
  } else if (dest_size_px.IsEmpty()) {
    wp_viewport_set_destination(viewport(), -1, -1);
  }
  display_size_px_ = dest_size_px;
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
