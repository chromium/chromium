// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_

#include "ui/gfx/font_render_params.h"
#include "ui/gfx/mojom/font_render_params.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::SubpixelRendering,
                  gfx::FontRenderParams::SubpixelRendering> {
  static gfx::mojom::SubpixelRendering ToMojom(
      gfx::FontRenderParams::SubpixelRendering input) {
    switch (input) {
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
        return gfx::mojom::SubpixelRendering::kNone;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
        return gfx::mojom::SubpixelRendering::kRGB;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
        return gfx::mojom::SubpixelRendering::kBGR;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
        return gfx::mojom::SubpixelRendering::kVRGB;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
        return gfx::mojom::SubpixelRendering::kVBGR;
    }
    NOTREACHED();
    return gfx::mojom::SubpixelRendering::kNone;
  }

  static bool FromMojom(gfx::mojom::SubpixelRendering input,
                        gfx::FontRenderParams::SubpixelRendering* out) {
    switch (input) {
      case gfx::mojom::SubpixelRendering::kNone:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
        return true;
      case gfx::mojom::SubpixelRendering::kRGB:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
        return true;
      case gfx::mojom::SubpixelRendering::kBGR:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
        return true;
      case gfx::mojom::SubpixelRendering::kVRGB:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
        return true;
      case gfx::mojom::SubpixelRendering::kVBGR:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
        return true;
    }
    *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::Hinting, gfx::FontRenderParams::Hinting> {
  static gfx::mojom::Hinting ToMojom(gfx::FontRenderParams::Hinting input) {
    switch (input) {
      case gfx::FontRenderParams::HINTING_NONE:
        return gfx::mojom::Hinting::kNone;
      case gfx::FontRenderParams::HINTING_SLIGHT:
        return gfx::mojom::Hinting::kSlight;
      case gfx::FontRenderParams::HINTING_MEDIUM:
        return gfx::mojom::Hinting::kMedium;
      case gfx::FontRenderParams::HINTING_FULL:
        return gfx::mojom::Hinting::kFull;
    }
    NOTREACHED();
    return gfx::mojom::Hinting::kNone;
  }

  static bool FromMojom(gfx::mojom::Hinting input,
                        gfx::FontRenderParams::Hinting* out) {
    switch (input) {
      case gfx::mojom::Hinting::kNone:
        *out = gfx::FontRenderParams::HINTING_NONE;
        return true;
      case gfx::mojom::Hinting::kSlight:
        *out = gfx::FontRenderParams::HINTING_SLIGHT;
        return true;
      case gfx::mojom::Hinting::kMedium:
        *out = gfx::FontRenderParams::HINTING_MEDIUM;
        return true;
      case gfx::mojom::Hinting::kFull:
        *out = gfx::FontRenderParams::HINTING_FULL;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_
