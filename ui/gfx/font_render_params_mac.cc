// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "ui/base/ui_base_features.h"

namespace gfx {

namespace {

// Returns params that match SkiaTextRenderer's default render settings.
FontRenderParams LoadDefaults() {
  FontRenderParams params;
  params.antialiasing = true;
  params.autohinter = false;
  params.use_bitmaps = true;
  params.subpixel_positioning = true;

  if (!base::FeatureList::IsEnabled(features::kCr2023MacFontSmoothing)) {
    params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;
    params.hinting = FontRenderParams::HINTING_NONE;
  } else {
    params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
    params.hinting = FontRenderParams::HINTING_MEDIUM;
  }

  return params;
}

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // TODO: Query the OS for font render settings instead of returning defaults.
  static const gfx::FontRenderParams params(LoadDefaults());
  return params;
}

float GetFontRenderParamsDeviceScaleFactor() {
  return 1.0;
}

}  // namespace gfx
