// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/notreached.h"

namespace gfx {

// static
SkFontLCDConfig::LCDOrder FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
    FontRenderParams::SubpixelRendering subpixel_rendering) {
  switch (subpixel_rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
      return SkFontLCDConfig::kRGB_LCDOrder;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return SkFontLCDConfig::kBGR_LCDOrder;
  }

  NOTREACHED();
  return SkFontLCDConfig::kRGB_LCDOrder;
}

// static
SkFontLCDConfig::LCDOrientation
FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
    FontRenderParams::SubpixelRendering subpixel_rendering) {
  switch (subpixel_rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
      return SkFontLCDConfig::kHorizontal_LCDOrientation;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return SkFontLCDConfig::kVertical_LCDOrientation;
  }

  NOTREACHED();
  return SkFontLCDConfig::kHorizontal_LCDOrientation;
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
