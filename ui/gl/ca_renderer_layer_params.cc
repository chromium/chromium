// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/ca_renderer_layer_params.h"

namespace ui {

CARendererLayerParams::CARendererLayerParams(
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
    bool is_render_pass_draw_quad)
    : is_clipped(is_clipped),
      clip_rect(clip_rect),
      rounded_corner_bounds(rounded_corner_bounds),
      sorting_context_id(sorting_context_id),
      transform(transform),
      io_surface(std::move(io_surface)),
      io_surface_color_space(io_surface_color_space),
      contents_rect(contents_rect),
      rect(rect),
      background_color(background_color),
      edge_aa_mask(edge_aa_mask),
      opacity(opacity),
      nearest_neighbor_filter(nearest_neighbor_filter),
      hdr_metadata(hdr_metadata),
      protected_video_type(protected_video_type),
      is_render_pass_draw_quad(is_render_pass_draw_quad) {}

CARendererLayerParams::CARendererLayerParams(
    const CARendererLayerParams& other) = default;
CARendererLayerParams::~CARendererLayerParams() = default;

}  // namespace ui
