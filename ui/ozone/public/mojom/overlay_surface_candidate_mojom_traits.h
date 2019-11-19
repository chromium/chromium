// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_MOJOM_OVERLAY_SURFACE_CANDIDATE_MOJOM_TRAITS_H_
#define UI_OZONE_PUBLIC_MOJOM_OVERLAY_SURFACE_CANDIDATE_MOJOM_TRAITS_H_

#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/overlay_transform_mojom_traits.h"
#include "ui/ozone/public/mojom/overlay_surface_candidate.mojom.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace mojo {

template <>
struct EnumTraits<ui::ozone::mojom::OverlayStatus, ui::OverlayStatus> {
  static ui::ozone::mojom::OverlayStatus ToMojom(ui::OverlayStatus format) {
    switch (format) {
      case ui::OverlayStatus::OVERLAY_STATUS_PENDING:
        return ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_PENDING;
      case ui::OverlayStatus::OVERLAY_STATUS_ABLE:
        return ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_ABLE;
      case ui::OverlayStatus::OVERLAY_STATUS_NOT:
        return ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_NOT;
    }
    NOTREACHED();
    return ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_NOT;
  }

  static bool FromMojom(ui::ozone::mojom::OverlayStatus input,
                        ui::OverlayStatus* out) {
    switch (input) {
      case ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_PENDING:
        *out = ui::OverlayStatus::OVERLAY_STATUS_PENDING;
        return true;
      case ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_ABLE:
        *out = ui::OverlayStatus::OVERLAY_STATUS_ABLE;
        return true;
      case ui::ozone::mojom::OverlayStatus::OVERLAY_STATUS_NOT:
        *out = ui::OverlayStatus::OVERLAY_STATUS_NOT;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<ui::ozone::mojom::OverlaySurfaceCandidateDataView,
                    ui::OverlaySurfaceCandidate> {
  static const gfx::OverlayTransform& transform(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.transform;
  }

  static const gfx::BufferFormat& format(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.format;
  }

  static const gfx::Size& buffer_size(const ui::OverlaySurfaceCandidate& osc) {
    return osc.buffer_size;
  }

  static const gfx::RectF& display_rect(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.display_rect;
  }

  static const gfx::RectF& crop_rect(const ui::OverlaySurfaceCandidate& osc) {
    return osc.crop_rect;
  }

  static const gfx::Rect& clip_rect(const ui::OverlaySurfaceCandidate& osc) {
    return osc.clip_rect;
  }

  static bool is_clipped(const ui::OverlaySurfaceCandidate& osc) {
    return osc.is_clipped;
  }

  static bool is_opaque(const ui::OverlaySurfaceCandidate& osc) {
    return osc.is_opaque;
  }

  static int plane_z_order(const ui::OverlaySurfaceCandidate& osc) {
    return osc.plane_z_order;
  }

  static bool overlay_handled(const ui::OverlaySurfaceCandidate& osc) {
    return osc.overlay_handled;
  }

  static bool Read(ui::ozone::mojom::OverlaySurfaceCandidateDataView data,
                   ui::OverlaySurfaceCandidate* out) {
    out->is_clipped = data.is_clipped();
    out->is_opaque = data.is_opaque();
    out->plane_z_order = data.plane_z_order();
    out->overlay_handled = data.overlay_handled();
    return data.ReadTransform(&out->transform) &&
           data.ReadFormat(&out->format) &&
           data.ReadBufferSize(&out->buffer_size) &&
           data.ReadDisplayRect(&out->display_rect) &&
           data.ReadCropRect(&out->crop_rect) &&
           data.ReadClipRect(&out->clip_rect);
  }
};

}  // namespace mojo

#endif  // UI_OZONE_PUBLIC_MOJOM_OVERLAY_SURFACE_CANDIDATE_MOJOM_TRAITS_H_
