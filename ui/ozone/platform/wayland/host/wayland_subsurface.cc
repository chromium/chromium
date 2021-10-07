// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"

#include <cstdint>

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace {

// Returns DIP bounds of the subsurface relative to the parent surface.
gfx::Rect AdjustSubsurfaceBounds(const gfx::Rect& bounds_px,
                                 const gfx::Rect& parent_bounds_px,
                                 int32_t buffer_scale) {
  const auto bounds_dip =
      gfx::ScaleToEnclosingRect(bounds_px, 1.0 / buffer_scale);
  const auto parent_bounds_dip =
      gfx::ScaleToEnclosingRect(parent_bounds_px, 1.0 / buffer_scale);
  return wl::TranslateBoundsToParentCoordinates(bounds_dip, parent_bounds_dip);
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

void WaylandSubsurface::CreateSubsurface() {
  DCHECK(parent_);

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  subsurface_ = wayland_surface()->CreateSubsurface(parent_->root_surface());

  DCHECK(subsurface_);
  wl_subsurface_set_sync(subsurface_.get());

  // Subsurfaces don't need to trap input events. Its display rect is fully
  // contained in |parent_|'s. Setting input_region to empty allows |parent_| to
  // dispatch all of the input to platform window.
  wayland_surface()->SetInputRegion({});

  connection_->buffer_manager_host()->SetSurfaceConfigured(wayland_surface());
}

void WaylandSubsurface::ConfigureAndShowSurface(
    const gfx::Rect& bounds_px,
    const gfx::Rect& parent_bounds_px,
    int32_t buffer_scale,
    const WaylandSurface* reference_below,
    const WaylandSurface* reference_above) {
  Show();

  // Chromium positions quads in display::Display coordinates in physical
  // pixels, but Wayland requires them to be in local surface coordinates a.k.a
  // relative to parent window.
  auto bounds_dip_in_parent_surface =
      AdjustSubsurfaceBounds(bounds_px, parent_bounds_px, buffer_scale);
  wl_subsurface_set_position(subsurface_.get(),
                             bounds_dip_in_parent_surface.x(),
                             bounds_dip_in_parent_surface.y());

  // Setup the stacking order of this subsurface.
  DCHECK(!reference_above || !reference_below);
  if (reference_below) {
    wl_subsurface_place_above(subsurface_.get(), reference_below->surface());
  } else if (reference_above) {
    wl_subsurface_place_below(subsurface_.get(), reference_above->surface());
  }
}

}  // namespace ui
