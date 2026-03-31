// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_

#include "base/notreached.h"
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
  }

  static gfx::FontRenderParams::SubpixelRendering FromMojom(
      gfx::mojom::SubpixelRendering input) {
    switch (input) {
      case gfx::mojom::SubpixelRendering::kNone:
        return gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
      case gfx::mojom::SubpixelRendering::kRGB:
        return gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
      case gfx::mojom::SubpixelRendering::kBGR:
        return gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
      case gfx::mojom::SubpixelRendering::kVRGB:
        return gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
      case gfx::mojom::SubpixelRendering::kVBGR:
        return gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    }
    NOTREACHED();
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
  }

  static gfx::FontRenderParams::Hinting FromMojom(gfx::mojom::Hinting input) {
    switch (input) {
      case gfx::mojom::Hinting::kNone:
        return gfx::FontRenderParams::HINTING_NONE;
      case gfx::mojom::Hinting::kSlight:
        return gfx::FontRenderParams::HINTING_SLIGHT;
      case gfx::mojom::Hinting::kMedium:
        return gfx::FontRenderParams::HINTING_MEDIUM;
      case gfx::mojom::Hinting::kFull:
        return gfx::FontRenderParams::HINTING_FULL;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_FONT_RENDER_PARAMS_MOJOM_TRAITS_H_
