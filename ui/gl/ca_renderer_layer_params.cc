// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/ca_renderer_layer_params.h"

#include "base/logging.h"
#include "ui/gl/gl_image_io_surface.h"

namespace {

base::ScopedCFTypeRef<IOSurfaceRef> GLImageIOSurface(gl::GLImage* image) {
  if (auto* image_io_surface = gl::GLImageIOSurface::FromGLImage(image)) {
    return image_io_surface->io_surface();
  }
  return base::ScopedCFTypeRef<IOSurfaceRef>();
}

gfx::ColorSpace GLImageColorSpace(gl::GLImage* image) {
  if (auto* image_io_surface = gl::GLImageIOSurface::FromGLImage(image)) {
    return image_io_surface->color_space();
  }
  return gfx::ColorSpace();
}

}  // namespace
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
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter,
    absl::optional<gfx::HDRMetadata> hdr_metadata,
    gfx::ProtectedVideoType protected_video_type)
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
      filter(filter),
      hdr_metadata(hdr_metadata),
      protected_video_type(protected_video_type) {}

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
    unsigned filter,
    absl::optional<gfx::HDRMetadata> hdr_metadata,
    gfx::ProtectedVideoType protected_video_type)
    : CARendererLayerParams(is_clipped,
                            clip_rect,
                            rounded_corner_bounds,
                            sorting_context_id,
                            transform,
                            GLImageIOSurface(image),
                            GLImageColorSpace(image),
                            contents_rect,
                            rect,
                            background_color,
                            edge_aa_mask,
                            opacity,
                            filter,
                            hdr_metadata,
                            protected_video_type) {
  if (image && !io_surface) {
    DLOG(ERROR) << "Cannot schedule CALayer with non-IOSurface GLImage";
  }
}

CARendererLayerParams::CARendererLayerParams(
    const CARendererLayerParams& other) = default;
CARendererLayerParams::~CARendererLayerParams() = default;

}  // namespace ui
