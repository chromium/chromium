// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"

namespace gfx {

namespace {

// Returns params that match SkiaTextRenderer's default render settings.
FontRenderParams LoadDefaults() {
  FontRenderParams params;
  params.antialiasing = true;
  params.autohinter = false;
  params.use_bitmaps = true;
  params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
  params.subpixel_positioning = true;
  params.hinting = FontRenderParams::HINTING_MEDIUM;

  return params;
}

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // TODO: Query the OS for font render settings instead of returning defaults.
  static const base::NoDestructor<gfx::FontRenderParams> params(LoadDefaults());
  return *params;
}

}  // namespace gfx
