// Copyright 2016 The Chromium Authors. All rights reserved.
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
    gl::GLImage* image,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter)
    : is_clipped(is_clipped),
      clip_rect(clip_rect),
      rounded_corner_bounds(rounded_corner_bounds),
      sorting_context_id(sorting_context_id),
      transform(transform),
      image(image),
      contents_rect(contents_rect),
      rect(rect),
      background_color(background_color),
      edge_aa_mask(edge_aa_mask),
      opacity(opacity),
      filter(filter) {}

CARendererLayerParams::CARendererLayerParams(
    const CARendererLayerParams& other) = default;
CARendererLayerParams::~CARendererLayerParams() = default;

}  // namespace ui
