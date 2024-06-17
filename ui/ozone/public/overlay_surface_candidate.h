// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_
#define UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/overlay_type.h"

namespace ui {

enum OverlayStatus {
  OVERLAY_STATUS_PENDING,
  OVERLAY_STATUS_ABLE,
  OVERLAY_STATUS_NOT,
  OVERLAY_STATUS_LAST = OVERLAY_STATUS_NOT
};

class COMPONENT_EXPORT(OZONE_BASE) OverlaySurfaceCandidate {
 public:
  OverlaySurfaceCandidate();
  OverlaySurfaceCandidate(const OverlaySurfaceCandidate& other);
  ~OverlaySurfaceCandidate();
  OverlaySurfaceCandidate& operator=(const OverlaySurfaceCandidate& other);

  // Note that |clip_rect|, |overlay_handled|, |native_pixmap|, and
  // gfx::Transform variants of |transform| are *not* used as part of the
  // comparison.
  bool operator<(const OverlaySurfaceCandidate& other) const;

  // Transformation to apply to layer during composition.
  // Note: A |gfx::OverlayTransform| transforms the buffer within its bounds and
  // does not affect |display_rect|.
  absl::variant<gfx::OverlayTransform, gfx::Transform> transform =
      gfx::OVERLAY_TRANSFORM_NONE;
  // Format of the buffer to composite.
  gfx::BufferFormat format = gfx::BufferFormat::BGRA_8888;
  // Color space of the buffer
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  // Stacking order of the overlay plane relative to the main surface,
  // which is 0. Signed to allow for "underlays".
  int plane_z_order = 0;
  // Size of the buffer, in pixels.
  gfx::Size buffer_size;
  // Rect on the display to position the overlay to. Input rectangle may
  // not have integer coordinates, but when accepting for overlay, must
  // be modified by CheckOverlaySupport to output integer values.
  gfx::RectF display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF crop_rect;
  // If the quad is clipped, the clip rect in the target content space after
  // composition.
  std::optional<gfx::Rect> clip_rect;
  // If the quad doesn't require blending.
  bool is_opaque = false;
  // Opacity of the overlay independent of buffer alpha. When rendered:
  // src-alpha = |opacity| * buffer-component-alpha.
  float opacity = 1.0f;
  // Optionally contains a pointer to the NativePixmap corresponding to this
  // candidate.
  scoped_refptr<gfx::NativePixmap> native_pixmap;
  // A unique ID corresponding to |native_pixmap|. The ID is not reused even if
  // |native_pixmap| is destroyed. Zero if |native_pixmap| is null.
  // TODO(samans): This will not be necessary once Ozone/DRM not longer uses a
  // cache for overlay testing. https://crbug.com/1034559
  uint32_t native_pixmap_unique_id = 0;
  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled = false;
  // If this candidate requires an overlay for proper display.
  bool requires_overlay = false;
  // Hints for overlay prioritization when delegated composition is used.
  gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;
  // Specifies the rounded corners of overlay in radii.
  gfx::RRectF rounded_corners;
  // Specifies the background color of the overlay.
  std::optional<SkColor> background_color;
  // Specifies the type of of the overlay, which is proposed by a similarly
  // named strategy.
  gfx::OverlayType overlay_type = gfx::OverlayType::kSimple;
};

using OverlaySurfaceCandidateList = std::vector<OverlaySurfaceCandidate>;
using OverlayStatusList = std::vector<OverlayStatus>;

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_
