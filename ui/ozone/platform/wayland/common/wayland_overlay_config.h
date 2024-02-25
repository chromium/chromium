// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OVERLAY_CONFIG_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OVERLAY_CONFIG_H_

#include <memory>

#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"

namespace wl {

using BufferId = uint32_t;

struct WaylandOverlayConfig {
  WaylandOverlayConfig();
  WaylandOverlayConfig(WaylandOverlayConfig&& other);
  WaylandOverlayConfig(const gfx::OverlayPlaneData& data,
                       std::unique_ptr<gfx::GpuFence> fence,
                       BufferId buffer_id,
                       float scale_factor);
  WaylandOverlayConfig& operator=(WaylandOverlayConfig&& other);

  ~WaylandOverlayConfig();

  // Specifies the stacking order of this overlay plane, relative to primary
  // plane.
  int z_order = 0;

  // Specifies how the buffer is to be transformed during composition.
  // Note: A |gfx::OverlayTransform| transforms the buffer within its bounds and
  // does not affect |bounds_rect|.
  absl::variant<gfx::OverlayTransform, gfx::Transform> transform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;

  // Specifies if alpha blending, with premultiplied alpha should be applied at
  // scanout.
  bool enable_blend = false;

  // Specifies priority of this overlay if delegated composition is supported
  // and enabled.
  gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;

  // A unique id for the buffer, which is used to identify imported wl_buffers
  // on the browser process.
  uint32_t buffer_id = 0;

  // Scale factor of the GPU side surface with respect to a display where the
  // surface is located at.
  float surface_scale_factor = 1.f;

  // Specifies where it is supposed to be on the display in physical pixels.
  // This, after scaled by buffer_scale sets the destination rectangle of
  // Wayland Viewport.
  gfx::RectF bounds_rect;

  // Specifies the region within the buffer to be placed inside |bounds_rect|.
  // This sets the source rectangle of Wayland Viewport.
  gfx::RectF crop_rect = {1.f, 1.f};

  // Damage in viz::Display space, the same space as |bounds_rect|. Optional
  // to hint a partial swap.
  gfx::Rect damage_region;

  // Opacity of the overlay independent of buffer alpha.
  // Valid values are [0.0, 1.0f].
  float opacity = 1.f;

  // Specifies a GpuFenceHandle to be waited on before content of the buffer can
  // be accessed by the display controller for overlay, or by the gpu for
  // compositing.
  gfx::GpuFenceHandle access_fence_handle;

  // Specifies the color space data of the wayland config.
  std::optional<gfx::ColorSpace> color_space;

  // Specifies rounded clip bounds of the overlay if delegated composition is
  // supported and enabled.
  std::optional<gfx::RRectF> rounded_clip_bounds;

  // Optional: background color of this overlay plane.
  std::optional<SkColor4f> background_color;

  // Optional: clip rect for this overlay.
  std::optional<gfx::Rect> clip_rect;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OVERLAY_CONFIG_H_
