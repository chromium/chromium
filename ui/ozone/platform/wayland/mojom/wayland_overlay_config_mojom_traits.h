// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_
#define UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolor4f_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/gpu_fence_handle_mojom_traits.h"
#include "ui/gfx/mojom/overlay_priority_hint_mojom_traits.h"
#include "ui/gfx/mojom/overlay_transform_mojom_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom-shared.h"

namespace mojo {

template <>
struct UnionTraits<wl::mojom::TransformUnionDataView,
                   absl::variant<gfx::OverlayTransform, gfx::Transform>> {
  static wl::mojom::TransformUnionDataView::Tag GetTag(
      const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform);

  static gfx::OverlayTransform overlay_transform(
      const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform) {
    return absl::get<gfx::OverlayTransform>(transform);
  }

  static gfx::Transform matrix_transform(
      const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform) {
    return absl::get<gfx::Transform>(transform);
  }

  static bool Read(wl::mojom::TransformUnionDataView data,
                   absl::variant<gfx::OverlayTransform, gfx::Transform>* out);
};

template <>
struct StructTraits<wl::mojom::WaylandOverlayConfigDataView,
                    wl::WaylandOverlayConfig> {
  static int z_order(const wl::WaylandOverlayConfig& input) {
    return input.z_order;
  }

  static const std::optional<gfx::ColorSpace>& color_space(
      const wl::WaylandOverlayConfig& input) {
    return input.color_space;
  }

  static const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform(
      const wl::WaylandOverlayConfig& input) {
    return input.transform;
  }

  static uint32_t buffer_id(const wl::WaylandOverlayConfig& input) {
    return input.buffer_id;
  }

  static float surface_scale_factor(const wl::WaylandOverlayConfig& input) {
    return input.surface_scale_factor;
  }

  static const gfx::RectF& bounds_rect(const wl::WaylandOverlayConfig& input) {
    return input.bounds_rect;
  }

  static const gfx::RectF& crop_rect(const wl::WaylandOverlayConfig& input) {
    return input.crop_rect;
  }

  static const gfx::Rect& damage_region(const wl::WaylandOverlayConfig& input) {
    return input.damage_region;
  }

  static bool enable_blend(const wl::WaylandOverlayConfig& input) {
    return input.enable_blend;
  }

  static float opacity(const wl::WaylandOverlayConfig& input) {
    return input.opacity;
  }

  static gfx::GpuFenceHandle access_fence_handle(
      const wl::WaylandOverlayConfig& input) {
    return input.access_fence_handle.Clone();
  }

  static const gfx::OverlayPriorityHint& priority_hint(
      const wl::WaylandOverlayConfig& input) {
    return input.priority_hint;
  }

  static const std::optional<gfx::RRectF>& rounded_clip_bounds(
      const wl::WaylandOverlayConfig& input) {
    return input.rounded_clip_bounds;
  }

  static const std::optional<SkColor4f>& background_color(
      const wl::WaylandOverlayConfig& input) {
    return input.background_color;
  }

  static const std::optional<gfx::Rect>& clip_rect(
      const wl::WaylandOverlayConfig& input) {
    return input.clip_rect;
  }

  static bool Read(wl::mojom::WaylandOverlayConfigDataView data,
                   wl::WaylandOverlayConfig* out);
};

}  // namespace mojo

#endif  // UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_
