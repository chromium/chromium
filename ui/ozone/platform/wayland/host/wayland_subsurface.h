// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"

namespace ui {
class WaylandConnection;
class WaylandWindow;

// Wraps a wl_surface with a wl_subsurface role assigned. It is used to submit a
// buffer as a sub region of WaylandWindow.
class WaylandSubsurface {
 public:
  WaylandSubsurface(WaylandConnection* connection, WaylandWindow* parent);
  WaylandSubsurface(const WaylandSubsurface&) = delete;
  WaylandSubsurface& operator=(const WaylandSubsurface&) = delete;
  ~WaylandSubsurface();

  wl_surface* surface() const { return wayland_surface_.surface(); }
  int32_t buffer_scale() const { return wayland_surface_.buffer_scale(); }
  WaylandSurface* wayland_surface() { return &wayland_surface_; }
  gfx::Rect bounds_px() { return bounds_px_; }
  bool IsOpaque() const { return !enable_blend_; }

  gfx::AcceleratedWidget GetWidget() const;

  // Sets up wl_surface and wl_subsurface. Allows an overlay to be shown
  // correctly once a wl_buffer is attached.
  //   |transform|: specifies the wl_surface buffer_transform.
  //   |src_rect|: specifies the displayable content (wp_viewport.src) of
  //     upcoming attached buffers.
  //   |bounds_rect|: The contents of the source rectangle are scaled to the
  //     destination size (wp_viewport.dst).
  //   |enable_blend|: whether the wl_surface will be transluscent.
  //   |reference_below| & |reference_above|: this subsurface is taken from the
  //     subsurface stack and inserted back to be immediately below/above the
  //     reference subsurface.
  //
  // The coordinate transformations from buffer pixel coordinates up to the
  // surface-local coordinates happen in the following order:
  //   1. buffer_transform
  //   2. buffer_scale
  //   3. crop and scale of viewport
  void ConfigureAndShowSurface(gfx::OverlayTransform transform,
                               const gfx::RectF& src_rect,
                               const gfx::Rect& bounds_rect,
                               bool enable_blend,
                               const WaylandSurface* reference_below,
                               const WaylandSurface* reference_above);

  // Assigns wl_subsurface role to the wl_surface so it is visible when a
  // wl_buffer is attached.
  void Show();
  // Remove wl_subsurface role to make this invisible.
  void Hide();
  bool IsVisible() const;

 private:
  // Helper of Show(). It does the role-assigning to wl_surface.
  void CreateSubsurface();
  void SetBounds(const gfx::Rect& bounds);

  // Tells wayland compositor to update the opaque region according to
  // |enable_blend_| and |bounds_px_|.
  void UpdateOpaqueRegion();

  WaylandSurface wayland_surface_;
  wl::Object<wl_subsurface> subsurface_;

  WaylandConnection* const connection_;
  // |parent_| refers to the WaylandWindow whose wl_surface is the parent to
  // this subsurface.
  WaylandWindow* const parent_;

  // Pixel bounds within the display to position this subsurface.
  gfx::Rect bounds_px_;
  bool enable_blend_ = true;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_
