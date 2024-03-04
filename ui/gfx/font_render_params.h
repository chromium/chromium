// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_RENDER_PARAMS_H_
#define UI_GFX_FONT_RENDER_PARAMS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// A collection of parameters describing how text should be rendered on Linux.
struct GFX_EXPORT FontRenderParams {
  bool operator==(const FontRenderParams& other) const {
    return antialiasing == other.antialiasing &&
           text_contrast == other.text_contrast &&
           text_gamma == other.text_gamma &&
           subpixel_positioning == other.subpixel_positioning &&
           autohinter == other.autohinter && use_bitmaps == other.use_bitmaps &&
           hinting == other.hinting &&
           subpixel_rendering == other.subpixel_rendering;
  }

  // Level of hinting to be applied.
  enum Hinting {
    HINTING_NONE = 0,
    HINTING_SLIGHT,
    HINTING_MEDIUM,
    HINTING_FULL,

    HINTING_MAX = HINTING_FULL,
  };

  // Different subpixel orders to be used for subpixel rendering.
  enum SubpixelRendering {
    SUBPIXEL_RENDERING_NONE = 0,
    SUBPIXEL_RENDERING_RGB,
    SUBPIXEL_RENDERING_BGR,
    SUBPIXEL_RENDERING_VRGB,
    SUBPIXEL_RENDERING_VBGR,

    SUBPIXEL_RENDERING_MAX = SUBPIXEL_RENDERING_VBGR,
  };

  // Antialiasing (grayscale if |subpixel_rendering| is SUBPIXEL_RENDERING_NONE
  // and RGBA otherwise).
  bool antialiasing = true;

  // Should subpixel positioning (i.e. fractional X positions for glyphs) be
  // used?
  // TODO(derat): Remove this; we don't set it in the browser and mostly ignore
  // it in Blink: http://crbug.com/396659
  bool subpixel_positioning = true;

  // Should FreeType's autohinter be used (as opposed to Freetype's bytecode
  // interpreter, which uses fonts' own hinting instructions)?
  bool autohinter = false;

  // Should embedded bitmaps in fonts should be used?
  bool use_bitmaps = false;

  // Hinting level.
  Hinting hinting = HINTING_MEDIUM;

  // Whether subpixel rendering should be used or not, and if so, the display's
  // subpixel order.
  SubpixelRendering subpixel_rendering = SUBPIXEL_RENDERING_NONE;

  // Text contrast and gamma settings, defaulted to Skia's fixed values.
  float text_contrast = SK_GAMMA_CONTRAST;
  float text_gamma = SK_GAMMA_EXPONENT;

  static SkPixelGeometry SubpixelRenderingToSkiaPixelGeometry(
      SubpixelRendering subpixel_rendering);
};

// A query used to determine the appropriate FontRenderParams.
struct GFX_EXPORT FontRenderParamsQuery {
  FontRenderParamsQuery();
  FontRenderParamsQuery(const FontRenderParamsQuery& other);
  ~FontRenderParamsQuery();

  bool is_empty() const {
    return families.empty() && pixel_size <= 0 && point_size <= 0 && style < 0;
  }

  // Requested font families, or empty if unset.
  std::vector<std::string> families;

  // Font size in pixels or points, or 0 if unset.
  int pixel_size;
  int point_size;

  // Font::FontStyle bit field, or -1 if unset.
  int style;

  // Weight of the font. Weight::NORMAL by default.
  Font::Weight weight;

  // The device scale factor of the display, or 0 if unset.
  float device_scale_factor;
};

// Returns the appropriate parameters for rendering the font described by
// |query|. If |family_out| is non-NULL, it will be updated to contain the
// recommended font family from |query.families|.
GFX_EXPORT FontRenderParams GetFontRenderParams(
    const FontRenderParamsQuery& query,
    std::string* family_out);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
// Clears GetFontRenderParams()'s cache. Intended to be called by tests that are
// changing Fontconfig's configuration.
GFX_EXPORT void ClearFontRenderParamsCacheForTest();
#endif

// Gets the device scale factor to query the FontRenderParams.
GFX_EXPORT float GetFontRenderParamsDeviceScaleFactor();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
// Sets the device scale factor for FontRenderParams to decide
// if it should enable subpixel positioning.
GFX_EXPORT void SetFontRenderParamsDeviceScaleFactor(
    float device_scale_factor);
#endif

}  // namespace gfx

#endif  // UI_GFX_FONT_RENDER_PARAMS_H_
