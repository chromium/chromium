// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DisplayMode_Params::DisplayMode_Params() {}

DisplayMode_Params::~DisplayMode_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params(
    const DisplaySnapshot_Params& other) = default;

DisplaySnapshot_Params::~DisplaySnapshot_Params() {}

OverlayCheck_Params::OverlayCheck_Params() {}

OverlayCheck_Params::OverlayCheck_Params(
    const ui::OverlaySurfaceCandidate& candidate)
    : buffer_size(candidate.buffer_size),
      transform(candidate.transform),
      format(candidate.format),
      display_rect(gfx::ToNearestRect(candidate.display_rect)),
      crop_rect(candidate.crop_rect),
      is_opaque(candidate.is_opaque),
      plane_z_order(candidate.plane_z_order),
      is_overlay_candidate(candidate.overlay_handled) {}

OverlayCheck_Params::OverlayCheck_Params(const OverlayCheck_Params& other) =
    default;

OverlayCheck_Params::~OverlayCheck_Params() {}

}  // namespace ui
