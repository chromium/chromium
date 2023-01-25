// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"

namespace ui {
class WaylandConnection;
class WaylandWindow;

// Wraps a wl_surface with a wl_subsurface role assigned. It is used to submit a
// buffer as a sub region of WaylandWindow.
//
// Inherits base::LinkNode<> s.t. it's location in the subsurface stack can be
// tracked and prevent us from sending excessive wl_subsurface.place_below/above
// requests.
class WaylandSubsurface : public base::LinkNode<WaylandSubsurface> {
 public:
  WaylandSubsurface(WaylandConnection* connection, WaylandWindow* parent);
  WaylandSubsurface(const WaylandSubsurface&) = delete;
  WaylandSubsurface& operator=(const WaylandSubsurface&) = delete;
  ~WaylandSubsurface();

  wl_surface* surface() const { return wayland_surface_.surface(); }
  WaylandSurface* wayland_surface() { return &wayland_surface_; }

  gfx::AcceleratedWidget GetWidget() const;

  // Sets up wl_subsurface by setting the surface location coordinates and the
  // stacking order of this subsurface.
  //   |bounds_px|: The pixel bounds of this subsurface content in
  //     display::Display coordinates used by chrome.
  //   |parent_bounds_px|: Same as |bounds_px| but for the parent surface.
  //   |clip_rect_px|: The pixel bounds of this subsurface's clip rect in
  //     display::Display coordinates. Pass nullopt to unset the clip rect.
  //   |buffer_scale|: the scale factor of the next attached buffer.
  //   |reference_below| & |reference_above|: this subsurface is taken from the
  //     subsurface stack and inserted back to be immediately below/above the
  //     reference subsurface.
  void ConfigureAndShowSurface(const gfx::RectF& bounds_px,
                               const gfx::RectF& parent_bounds_px,
                               const absl::optional<gfx::Rect>& clip_rect_px,
                               float buffer_scale,
                               WaylandSubsurface* reference_below,
                               WaylandSubsurface* reference_above);

  // Assigns wl_subsurface role to the wl_surface so it is visible when a
  // wl_buffer is attached.
  void Show();
  // Remove wl_subsurface role to make this invisible.
  void Hide();
  bool IsVisible() const;

 private:
  // Helper of Show(). It does the role-assigning to wl_surface.
  void CreateSubsurface();

  WaylandSurface wayland_surface_;
  wl::Object<wl_subsurface> subsurface_;
  wl::Object<augmented_sub_surface> augmented_subsurface_;
  gfx::PointF position_dip_;
  absl::optional<gfx::RectF> clip_dip_;

  const raw_ptr<WaylandConnection> connection_;
  // |parent_| refers to the WaylandWindow whose wl_surface is the parent to
  // this subsurface.
  const raw_ptr<WaylandWindow, DanglingUntriaged> parent_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SUBSURFACE_H_
