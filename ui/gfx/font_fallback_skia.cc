// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback_skia_impl.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

std::vector<Font> GetFallbackFonts(const Font& font) {
  return std::vector<Font>();
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  if (text.empty())
    return false;

  sk_sp<SkTypeface> fallback_typeface =
      GetSkiaFallbackTypeface(font, locale, text);

  if (!fallback_typeface)
    return false;

  // Fallback needs to keep the exact SkTypeface, as re-matching the font using
  // family name and styling information loses access to the underlying platform
  // font handles and is not guaranteed to result in the correct typeface, see
  // https://crbug.com/1003829
  *result = Font(PlatformFont::CreateFromSkTypeface(
      std::move(fallback_typeface), font.GetFontSize(), base::nullopt));
  return true;
}

}  // namespace gfx
