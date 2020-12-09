// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_
#define UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_

#include <vector>

#include "base/component_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/overlay_transform.h"

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

  // Note that |clip_rect|, |is_clipped|, |overlay_handled| and |native_pixmap|
  // are *not* used as part of the comparison.
  bool operator<(const OverlaySurfaceCandidate& other) const;

  // Transformation to apply to layer during composition.
  gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
  // Format of the buffer to composite.
  gfx::BufferFormat format = gfx::BufferFormat::BGRA_8888;
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
  // Clip rect in the target content space after composition.
  gfx::Rect clip_rect;
  // If the quad is clipped after composition.
  bool is_clipped = false;
  // If the quad doesn't require blending.
  bool is_opaque = false;
  // Optionally contains a pointer to the NativePixmap corresponding to this
  // candidate.
  scoped_refptr<gfx::NativePixmap> native_pixmap = nullptr;
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
};

using OverlaySurfaceCandidateList = std::vector<OverlaySurfaceCandidate>;
using OverlayStatusList = std::vector<OverlayStatus>;

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_SURFACE_CANDIDATE_H_
