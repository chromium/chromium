// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_FALLBACK_LINUX_H_
#define UI_GFX_FONT_FALLBACK_LINUX_H_

#include <string>

#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Exposed fallback font caches methods for testing.
GFX_EXPORT size_t GetFallbackFontEntriesCacheSizeForTesting();
GFX_EXPORT size_t GetFallbackFontListCacheSizeForTesting();
GFX_EXPORT void ClearAllFontFallbackCachesForTesting();

// Return a font family which provides a glyph for the Unicode code point
// specified by character.
//   c: a UTF-32 code point
//   preferred_locale: preferred locale identifier for the |characters|
//                     (e.g. "en", "ja", "zh-CN")
//
// Returns: the font family instance. The instance has an empty font name if the
// request could not be satisfied.
//
// Previously blink::WebFontInfo::fallbackFontForChar.
struct FallbackFontData {
  std::string name;
  // TODO(etienneb): This field should be a base::FilePath and renamed
  // |filepath|.
  std::string filename;
  int ttc_index = 0;
  bool is_bold = false;
  bool is_italic = false;
};
GFX_EXPORT FallbackFontData
GetFallbackFontForChar(UChar32 c, const std::string& preferred_locale);

}  // namespace gfx

#endif  // UI_GFX_FONT_FALLBACK_LINUX_H_
