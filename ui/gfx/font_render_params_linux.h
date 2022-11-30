// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_RENDER_PARAMS_LINUX_H_
#define UI_GFX_FONT_RENDER_PARAMS_LINUX_H_

#include "ui/gfx/font_render_params.h"

namespace gfx {

// Queries Fontconfig for rendering settings and updates |params_out| and
// |family_out| (if non-nullptr). Returns false on failure.
GFX_EXPORT bool QueryFontconfig(const FontRenderParamsQuery& query,
                                FontRenderParams* params_out,
                                std::string* family_out);

}  // namespace gfx

#endif  // UI_GFX_FONT_RENDER_PARAMS_LINUX_H_