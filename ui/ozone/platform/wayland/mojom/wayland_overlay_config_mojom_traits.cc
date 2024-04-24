// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config_mojom_traits.h"

#include <string_view>

#include "components/crash/core/common/crash_key.h"

namespace mojo {

namespace {

void SetDeserializationCrashKeyString(std::string_view str) {
  static crash_reporter::CrashKeyString<128> key("wayland_deserialization");
  key.Set(str);
}

}  // namespace

// static
wl::mojom::TransformUnionDataView::Tag
UnionTraits<wl::mojom::TransformUnionDataView,
            absl::variant<gfx::OverlayTransform, gfx::Transform>>::
    GetTag(
        const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform) {
  if (absl::holds_alternative<gfx::OverlayTransform>(transform)) {
    return wl::mojom::TransformUnionDataView::Tag::kOverlayTransform;
  }
  return wl::mojom::TransformUnionDataView::Tag::kMatrixTransform;
}

// static
bool UnionTraits<wl::mojom::TransformUnionDataView,
                 absl::variant<gfx::OverlayTransform, gfx::Transform>>::
    Read(wl::mojom::TransformUnionDataView data,
         absl::variant<gfx::OverlayTransform, gfx::Transform>* out) {
  switch (data.tag()) {
    case wl::mojom::TransformUnionDataView::Tag::kOverlayTransform:
      gfx::OverlayTransform overlay_transform;
      if (!data.ReadOverlayTransform(&overlay_transform)) {
        return false;
      }
      *out = overlay_transform;
      return true;
    case wl::mojom::TransformUnionDataView::Tag::kMatrixTransform:
      gfx::Transform matrix_transform;
      if (!data.ReadMatrixTransform(&matrix_transform)) {
        return false;
      }
      *out = matrix_transform;
      return true;
  }
}

// static
bool StructTraits<wl::mojom::WaylandOverlayConfigDataView,
                  wl::WaylandOverlayConfig>::
    Read(wl::mojom::WaylandOverlayConfigDataView data,
         wl::WaylandOverlayConfig* out) {
  out->z_order = data.z_order();

  if (!data.ReadColorSpace(&out->color_space))
    return false;

  if (!data.ReadTransform(&out->transform))
    return false;

  out->buffer_id = data.buffer_id();

  if (data.surface_scale_factor() <= 0) {
    SetDeserializationCrashKeyString("Invalid surface scale factor.");
    return false;
  }

  out->surface_scale_factor = data.surface_scale_factor();

  if (!data.ReadBoundsRect(&out->bounds_rect))
    return false;
  if (!data.ReadCropRect(&out->crop_rect))
    return false;
  if (!data.ReadDamageRegion(&out->damage_region))
    return false;

  out->enable_blend = data.enable_blend();

  if (data.opacity() < 0 || data.opacity() > 1.f) {
    SetDeserializationCrashKeyString("Invalid opacity value.");
    return false;
  }

  out->opacity = data.opacity();

  if (!data.ReadAccessFenceHandle(&out->access_fence_handle))
    return false;
  if (!data.ReadPriorityHint(&out->priority_hint))
    return false;
  if (!data.ReadRoundedClipBounds(&out->rounded_clip_bounds))
    return false;
  if (!data.ReadBackgroundColor(&out->background_color))
    return false;
  if (!data.ReadClipRect(&out->clip_rect))
    return false;

  return true;
}

}  // namespace mojo
