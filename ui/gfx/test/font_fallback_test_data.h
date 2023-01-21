// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_
#define UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_

#include <string>
#include <vector>

#include "third_party/icu/source/common/unicode/uscript.h"

namespace gfx {

// A font test case for the parameterized unittests.
struct FallbackFontTestCase {
  FallbackFontTestCase();
  FallbackFontTestCase(UScriptCode script_arg,
                       std::string language_tag_arg,
                       std::u16string text_arg,
                       std::vector<std::string> fallback_fonts_arg);
  FallbackFontTestCase(const FallbackFontTestCase& other);
  ~FallbackFontTestCase();
  UScriptCode script;
  std::string language_tag;
  std::u16string text;
  std::vector<std::string> fallback_fonts;
};

extern const std::vector<FallbackFontTestCase> kGetFontFallbackTests;

}  // namespace gfx

#endif  // UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_
