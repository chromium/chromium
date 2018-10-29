// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"

namespace gfx {

namespace {

// Returns the system's default settings.
FontRenderParams LoadDefaults() {
  FontRenderParams params;
  params.antialiasing = true;
  params.autohinter = true;
  params.use_bitmaps = true;
  params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;
  params.subpixel_positioning = true;
  params.hinting = FontRenderParams::HINTING_SLIGHT;

  return params;
}

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // Customized font rendering settings are not supported, only defaults.
  static const base::NoDestructor<gfx::FontRenderParams> params(LoadDefaults());
  return *params;
}

}  // namespace gfx
