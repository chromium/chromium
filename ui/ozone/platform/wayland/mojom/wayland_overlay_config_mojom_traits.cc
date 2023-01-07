// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config_mojom_traits.h"

#include "components/crash/core/common/crash_key.h"

namespace mojo {

namespace {

void SetDeserializationCrashKeyString(base::StringPiece str) {
  static crash_reporter::CrashKeyString<128> key("wayland_deserialization");
  key.Set(str);
}

}  // namespace

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
