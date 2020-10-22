// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include <cstdint>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Wrapper of a wl_surface, owned by a WaylandWindow or a WlSubsurface.
class WaylandSurface {
 public:
  WaylandSurface(WaylandConnection* connection, WaylandWindow* root_window);
  WaylandSurface(const WaylandSurface&) = delete;
  WaylandSurface& operator=(const WaylandSurface&) = delete;
  ~WaylandSurface();

  WaylandWindow* root_window() const { return root_window_; }
  wl_surface* surface() const { return surface_.get(); }
  wp_viewport* viewport() const { return viewport_.get(); }
  int32_t buffer_scale() const { return buffer_scale_; }
  void set_buffer_scale(int32_t scale) { buffer_scale_ = scale; }

  // Returns an id that identifies the |wl_surface_|.
  uint32_t GetSurfaceId() const;
  // Returns a gfx::AcceleratedWidget that identifies the WaylandWindow that
  // this WaylandSurface belongs to.
  gfx::AcceleratedWidget GetWidget() const;

  // Initializes the WaylandSurface and returns true iff success.
  // This may return false if a wl_surface could not be created, for example.
  bool Initialize();

  // Unsets |root_window_|. This is intended to be used in special cases, where
  // the underlying wl_surface must be kept alive with no root window associated
  // (e.g: window/tab dragging sessions).
  void UnsetRootWindow();

  // Attaches the given wl_buffer to the underlying wl_surface at (0, 0).
  void AttachBuffer(wl_buffer* buffer);

  // Describes where the surface needs to be repainted according to
  // |buffer_pending_damage_region|, which should be in buffer coordinates (px).
  void UpdateBufferDamageRegion(const gfx::Rect& buffer_pending_damage_region,
                                const gfx::Size& buffer_size);

  // Commits the underlying wl_surface.
  void Commit();

  // Sets an optional transformation for how the Wayland compositor interprets
  // the contents of the buffer attached to this surface.
  void SetBufferTransform(gfx::OverlayTransform transform);

  // Sets the buffer scale for this surface.
  void SetBufferScale(int32_t scale, bool update_bounds);

  // Sets the region that is opaque on this surface in physical pixels. This is
  // expected to be called whenever the region that the surface span changes or
  // the opacity changes.
  void SetOpaqueRegion(const gfx::Rect& bounds_px);

  // Set the source rectangle of the associated wl_surface.
  // See:
  // https://cgit.freedesktop.org/wayland/wayland-protocols/tree/stable/viewporter/viewporter.xml
  // If |src_rect| is empty, the source rectangle is unset.
  void SetViewportSource(const gfx::RectF& src_rect);

  // Set the destination size of the associated wl_surface according to
  // |dest_size_px|, which should be in physical pixels.
  void SetViewportDestination(const gfx::Size& dest_size_px);

  // Creates a wl_subsurface relating this surface and a parent surface,
  // |parent|. Callers take ownership of the wl_subsurface.
  wl::Object<wl_subsurface> CreateSubsurface(WaylandSurface* parent);

 private:
  WaylandConnection* const connection_;
  WaylandWindow* root_window_ = nullptr;
  wl::Object<wl_surface> surface_;
  wl::Object<wp_viewport> viewport_;

  // Transformation for how the compositor interprets the contents of the
  // buffer.
  gfx::OverlayTransform buffer_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // Wayland's scale factor for the output that this surface currently belongs
  // to.
  int32_t buffer_scale_ = 1;

  // Following fields are used to help determine the damage_region in
  // surface-local coordinates if wl_surface_damage_buffer() is not available.
  // Normalized bounds of the buffer to be displayed in |display_size_px_|.
  // If empty, no cropping is applied.
  gfx::RectF crop_rect_ = gfx::RectF();

  // Current size of the destination of the viewport in physical pixels. Wayland
  // compositor will scale the (cropped) buffer content to fit the
  // |display_size_px_|.
  // If empty, no scaling is applied.
  gfx::Size display_size_px_ = gfx::Size();

  // wl_surface_listener
  static void Enter(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);
  static void Leave(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
