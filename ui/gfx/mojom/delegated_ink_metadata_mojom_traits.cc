// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/delegated_ink_metadata_mojom_traits.h"
#include "base/time/time.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::DelegatedInkMetadataDataView,
                  std::unique_ptr<gfx::DelegatedInkMetadata>>::
    Read(gfx::mojom::DelegatedInkMetadataDataView data,
         std::unique_ptr<gfx::DelegatedInkMetadata>* out) {
  // Diameter isn't expected to ever be below 0, so stop here if it is in order
  // to avoid unexpected calculations in viz.
  if (data.diameter() < 0)
    return false;

  gfx::PointF point;
  base::TimeTicks timestamp;
  gfx::RectF presentation_area;
  SkColor color;
  base::TimeTicks frame_time;
  if (!data.ReadPoint(&point) || !data.ReadTimestamp(&timestamp) ||
      !data.ReadPresentationArea(&presentation_area) ||
      !data.ReadColor(&color) || !data.ReadFrameTime(&frame_time)) {
    return false;
  }
  // Render pass id is not set in the renderer process, therefore does not need
  // to be serialized by mojo.
  *out = std::make_unique<gfx::DelegatedInkMetadata>(
      point, data.diameter(), color, timestamp, presentation_area, frame_time,
      data.is_hovering(), /*render_pass_id=*/0);
  return true;
}

}  // namespace mojo
