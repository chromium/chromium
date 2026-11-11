// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_CA_RENDERER_LAYER_PARAMS_H_
#define UI_GL_CA_RENDERER_LAYER_PARAMS_H_

#include <array>
#include <optional>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_export.h"

namespace ui {

// Mirrors core animation layer edge constants.
enum CALayerEdge : uint32_t {
  kLayerEdgeLeft = 0x1,
  kLayerEdgeRight = 0x2,
  kLayerEdgeBottom = 0x4,
  kLayerEdgeTop = 0x8,
};

struct GL_EXPORT CABackdropBlurFilter {
  // Blur radius in RenderPass local space (scaled by `filters_scale`, before
  // `CARendererLayerParams::transform` is applied by TransformLayer).
  float blur_radius = 0.0f;

  friend bool operator==(const CABackdropBlurFilter&,
                         const CABackdropBlurFilter&) = default;
};

struct GL_EXPORT CABackdropColorFilter {
  // Optional 4x5 row-major color matrix matching CAColorMatrix.
  std::array<float, 20> color_matrix;

  friend bool operator==(const CABackdropColorFilter&,
                         const CABackdropColorFilter&) = default;
};

// A single CoreAnimation hardware-accelerated CSS backdrop-filter operation.
// Requires `CARendererLayerParams::opacity == 1.0f` because sibling backdrop
// and content CALayers do not form an isolated group for non-unit opacity.
using CABackdropFilter =
    std::variant<CABackdropBlurFilter, CABackdropColorFilter>;

struct GL_EXPORT CARendererLayerParams {
  CARendererLayerParams(
      bool is_clipped,
      const gfx::Rect clip_rect,
      const gfx::RRectF rounded_corner_bounds,
      unsigned sorting_context_id,
      const gfx::Transform& transform,
      gfx::ScopedIOSurface io_surface,
      const gfx::ColorSpace& io_surface_color_space,
      const gfx::RectF& contents_rect,
      const gfx::Rect& rect,
      SkColor4f background_color,
      unsigned edge_aa_mask,
      float opacity,
      bool nearest_neighbor_filter,
      const gfx::HDRMetadata& hdr_metadata,
      gfx::ProtectedVideoType protected_video_type,
      bool is_render_pass_draw_quad,
      std::vector<CABackdropFilter> backdrop_filters = {},
      std::optional<gfx::RRectF> backdrop_filter_bounds = std::nullopt,
      std::optional<gfx::RectF> backdrop_filter_clip = std::nullopt);
  CARendererLayerParams(const CARendererLayerParams& other);
  ~CARendererLayerParams();

  bool is_clipped;
  const gfx::Rect clip_rect;
  const gfx::RRectF rounded_corner_bounds;
  unsigned sorting_context_id;
  const gfx::Transform transform;
  gfx::ScopedIOSurface io_surface;
  gfx::ColorSpace io_surface_color_space;
  const gfx::RectF contents_rect;
  const gfx::Rect rect;
  SkColor4f background_color;
  unsigned edge_aa_mask;
  float opacity;
  bool nearest_neighbor_filter;
  gfx::HDRMetadata hdr_metadata;
  gfx::ProtectedVideoType protected_video_type;
  bool is_render_pass_draw_quad;
  std::vector<CABackdropFilter> backdrop_filters;
  // Clip bounds in RenderPass local space. Must be representable via native
  // CALayer cornerRadius + maskedCorners. If std::nullopt, defaults to `rect`.
  std::optional<gfx::RRectF> backdrop_filter_bounds;
  std::optional<gfx::RectF> backdrop_filter_clip;
};

}  // namespace ui

#endif  // UI_GL_CA_RENDERER_LAYER_PARAMS_H_
