// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"

#include <cstdint>

#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace {

gfx::Rect AdjustSubsurfaceBounds(const gfx::Rect& bounds_px,
                                 const gfx::Rect& parent_bounds_px,
                                 float ui_scale,
                                 int32_t parent_buffer_scale) {
  // TODO(fangzhoug): Verify the correctness of using ui_scale here, and in
  // other ozone wayland files.
  // Currently, the subsurface tree is at most 1 depth, gpu already sees buffer
  // bounds in the root_surface-local coordinates. So translation is not
  // needed for now.
  const auto bounds_dip = gfx::ScaleToRoundedRect(bounds_px, 1.0 / ui_scale);
  return gfx::ScaleToRoundedRect(bounds_dip, ui_scale / parent_buffer_scale);
}

}  // namespace

namespace ui {

WaylandSubsurface::WaylandSubsurface(WaylandConnection* connection,
                                     WaylandWindow* parent)
    : wayland_surface_(connection, parent),
      connection_(connection),
      parent_(parent) {
  DCHECK(parent_);
  DCHECK(connection_);
  if (!surface()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return;
  }
  wayland_surface_.Initialize();
}

WaylandSubsurface::~WaylandSubsurface() = default;

gfx::AcceleratedWidget WaylandSubsurface::GetWidget() const {
  return wayland_surface_.GetWidget();
}

void WaylandSubsurface::Show() {
  if (!subsurface_)
    CreateSubsurface();
}

void WaylandSubsurface::Hide() {
  if (!subsurface_)
    return;

  subsurface_.reset();
  connection_->buffer_manager_host()->ResetSurfaceContents(wayland_surface());
}

bool WaylandSubsurface::IsVisible() const {
  return !!subsurface_;
}

void WaylandSubsurface::UpdateOpaqueRegion() {
  gfx::Rect region_px =
      enable_blend_ ? gfx::Rect() : gfx::Rect(bounds_px_.size());
  wayland_surface()->SetOpaqueRegion(region_px);
}

void WaylandSubsurface::SetBounds(const gfx::Rect& bounds) {
  if (bounds_px_ == bounds)
    return;

  bounds_px_ = bounds;
  if (IsVisible()) {
    // Translate location from screen to surface coordinates.
    auto bounds_px =
        AdjustSubsurfaceBounds(bounds_px_, parent_->GetBounds(),
                               parent_->ui_scale(), parent_->buffer_scale());
    wl_subsurface_set_position(subsurface_.get(), bounds_px.x(), bounds_px.y());
  }
}

void WaylandSubsurface::CreateSubsurface() {
  DCHECK(parent_);

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  subsurface_ = wayland_surface()->CreateSubsurface(parent_->root_surface());

  // Chromium positions quads in display::Display coordinates in physical
  // pixels, but Wayland requires them to be in local surface coordinates a.k.a
  // relative to parent window.
  auto bounds_px =
      AdjustSubsurfaceBounds(bounds_px_, parent_->GetBounds(),
                             parent_->ui_scale(), parent_->buffer_scale());

  DCHECK(subsurface_);
  wl_subsurface_set_position(subsurface_.get(), bounds_px.x(), bounds_px.y());
  wl_subsurface_set_sync(subsurface_.get());

  // Subsurfaces don't need to trap input events. Its display rect is fully
  // contained in |parent_|'s. Setting input_region to empty allows |parent_| to
  // dispatch all of the input to platform window.
  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));
  wl_region_add(region.get(), 0, 0, 0, 0);
  wl_surface_set_input_region(surface(), region.get());

  connection_->buffer_manager_host()->SetSurfaceConfigured(wayland_surface());
}

void WaylandSubsurface::ConfigureAndShowSurface(
    gfx::OverlayTransform transform,
    const gfx::RectF& src_rect,
    const gfx::Rect& bounds_rect,
    bool enable_blend,
    const WaylandSurface* reference_below,
    const WaylandSurface* reference_above) {
  wayland_surface()->SetBufferTransform(transform);
  wayland_surface()->SetBufferScale(parent_->buffer_scale(), false);

  auto old_bounds = bounds_px_;
  SetBounds(bounds_rect);

  if (old_bounds != bounds_px_ || enable_blend_ != enable_blend) {
    enable_blend_ = enable_blend;
    UpdateOpaqueRegion();
  }

  Show();

  DCHECK(!reference_above || !reference_below);
  if (reference_below) {
    wl_subsurface_place_above(subsurface_.get(), reference_below->surface());
  } else if (reference_above) {
    wl_subsurface_place_below(subsurface_.get(), reference_above->surface());
  }

  wayland_surface()->SetViewportSource(src_rect);
  wayland_surface()->SetViewportDestination(bounds_rect.size());
}

}  // namespace ui
