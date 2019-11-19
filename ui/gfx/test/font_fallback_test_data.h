// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_
#define UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/uscript.h"

namespace gfx {

// A font test case for the parameterized unittests.
struct FallbackFontTestCase {
  FallbackFontTestCase();
  FallbackFontTestCase(UScriptCode script_arg,
                       std::string language_tag_arg,
                       base::string16 text_arg,
                       std::vector<std::string> fallback_fonts_arg,
                       bool is_win10_arg = false);
  FallbackFontTestCase(const FallbackFontTestCase& other);
  ~FallbackFontTestCase();
  UScriptCode script;
  std::string language_tag;
  base::string16 text;
  std::vector<std::string> fallback_fonts;
  bool is_win10 = false;
};

extern std::vector<FallbackFontTestCase> kGetFontFallbackTests;

}  // namespace gfx

#endif  // UI_GFX_TEST_FONT_FALLBACK_TEST_DATA_H_
