// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include <cstdint>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
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

  // Damages the surface according to |pending_damage_region|, which should be
  // in surface coordinates (dp).
  void Damage(const gfx::Rect& pending_damage_region);

  // Commits the underlying wl_surface.
  void Commit();

  // Sets the buffer scale for this surface.
  void SetBufferScale(int32_t scale, bool update_bounds);

  // Sets the bounds on this surface. This is used for determining the opaque
  // region.
  void SetBounds(const gfx::Rect& bounds_px);

  // Creates a wl_subsurface relating this surface and a parent surface,
  // |parent|. Callers take ownership of the wl_subsurface.
  wl::Object<wl_subsurface> CreateSubsurface(WaylandSurface* parent);

 private:
  WaylandConnection* const connection_;
  WaylandWindow* root_window_ = nullptr;
  wl::Object<wl_surface> surface_;

  // Wayland's scale factor for the output that this window currently belongs
  // to.
  int32_t buffer_scale_ = 1;

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
