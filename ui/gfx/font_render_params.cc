// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/notreached.h"

namespace gfx {

// static
SkPixelGeometry FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
    FontRenderParams::SubpixelRendering subpixel_rendering) {
  switch (subpixel_rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
      return kRGB_H_SkPixelGeometry;  // why not kUnknown_SkPixelGeometry ??
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
      return kRGB_H_SkPixelGeometry;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
      return kRGB_V_SkPixelGeometry;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
      return kBGR_H_SkPixelGeometry;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return kBGR_V_SkPixelGeometry;
  }

  NOTREACHED_IN_MIGRATION();
  return kRGB_H_SkPixelGeometry;
}

FontRenderParamsQuery::FontRenderParamsQuery()
    : pixel_size(0),
      point_size(0),
      style(-1),
      weight(Font::Weight::INVALID),
      device_scale_factor(0) {}

FontRenderParamsQuery::FontRenderParamsQuery(
    const FontRenderParamsQuery& other) = default;

FontRenderParamsQuery::~FontRenderParamsQuery() {}

}  // namespace gfx
