// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_FALLBACK_LINUX_H_
#define UI_GFX_FONT_FALLBACK_LINUX_H_

#include <string>

#include "base/files/file_path.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Exposed fallback font caches methods for testing.
GFX_EXPORT size_t GetFallbackFontEntriesCacheSizeForTesting();
GFX_EXPORT size_t GetFallbackFontListCacheSizeForTesting();
GFX_EXPORT void ClearAllFontFallbackCachesForTesting();

struct GFX_EXPORT FallbackFontData {
  std::string name;
  base::FilePath filepath;
  int fontconfig_interface_id = 0;
  int ttc_index = 0;
  bool is_bold = false;
  bool is_italic = false;

  FallbackFontData();
  FallbackFontData(const FallbackFontData& other);
  FallbackFontData& operator=(const FallbackFontData& other);
};

// Return a font family which provides a glyph for the Unicode code point
// specified by character.
//   c: an UTF-32 code point
//   preferred_locale: preferred locale identifier for |c|
//                     (e.g. "en", "ja", "zh-CN")
//
// Return whether the request was successful or not.
GFX_EXPORT bool GetFallbackFontForChar(UChar32 c,
                                       const std::string& preferred_locale,
                                       FallbackFontData* fallback_font);

}  // namespace gfx

#endif  // UI_GFX_FONT_FALLBACK_LINUX_H_
