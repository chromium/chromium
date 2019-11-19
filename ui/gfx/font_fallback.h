// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_FALLBACK_H_
#define UI_GFX_FONT_FALLBACK_H_

#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Font;

// Given a font, returns the fonts that are suitable for fallback.
GFX_EXPORT std::vector<Font> GetFallbackFonts(const Font& font);

// Finds a fallback font to render the specified |text| with respect to an
// initial |font|. Returns the resulting font via out param |result|. Returns
// |true| if a fallback font was found.
bool GFX_EXPORT GetFallbackFont(const Font& font,
                                const std::string& locale,
                                base::StringPiece16 text,
                                Font* result);

}  // namespace gfx

#endif  // UI_GFX_FONT_FALLBACK_H_
