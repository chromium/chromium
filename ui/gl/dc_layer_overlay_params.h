// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_LAYER_OVERLAY_PARAMS_H_
#define UI_GL_DC_LAYER_OVERLAY_PARAMS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/dc_layer_overlay_image.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GL_EXPORT DCLayerOverlayParams {
  DCLayerOverlayParams();
  ~DCLayerOverlayParams();

  // Image to display in overlay - could be hardware or software video frame,
  // swap chain, or dcomp surface. If null and |background_color| is present,
  // then this overlay will represents a solid color quad. If both this and
  // |background_color| are null, this overlay will not have any visible output.
  std::optional<DCLayerOverlayImage> overlay_image;

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of |overlay_image| to display in pixels. Ignored, if this overlay
  // represents a solid color. Usually integral, but can be non-integral in the
  // case of combining occlusion with scaling.
  gfx::RectF content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If present, then clip to |clip_rect| in root target space.
  std::optional<gfx::Rect> clip_rect;

  // When false, this overlay will be scaled with linear sampling.
  bool nearest_neighbor_filter = false;

  float opacity = 1.0;

  // The rounded corner bounds, in root target space
  gfx::RRectF rounded_corner_bounds;

  // If present, the overlay will contain this color as a background fill,
  // blended behind |overlay_image|.
  std::optional<SkColor4f> background_color;

  // Used to detect when multiple overlays are part of the same tile layer.
  uint64_t aggregated_layer_id = 0;

  // Parameters for video overlays, only used by |SwapChainPresenter|.
  struct VideoParams {
    VideoParams();
    ~VideoParams();

    gfx::ProtectedVideoType protected_video_type =
        gfx::ProtectedVideoType::kClear;

    gfx::ColorSpace color_space;

    gfx::HDRMetadata hdr_metadata;

    // Indication of the overlay to be detected as possible full screen
    // letterboxing.
    // Go to viz::OverlayCandidate::possible_video_fullscreen_letterboxing for
    // the details.
    bool possible_video_fullscreen_letterboxing = false;
  };

  VideoParams video_params;
};

}  // namespace gl

#endif  // UI_GL_DC_LAYER_OVERLAY_PARAMS_H_
