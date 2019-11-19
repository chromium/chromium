// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_FALLBACK_WIN_H_
#define UI_GFX_FONT_FALLBACK_WIN_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"

namespace gfx {

// Internals of font_fallback_win.cc exposed for testing.
namespace internal {

// Parses comma separated SystemLink |entry|, per the format described here:
// http://msdn.microsoft.com/en-us/goglobal/bb688134.aspx
//
// Sets |filename| and |font_name| respectively. If a field is not present
// or could not be parsed, the corresponding parameter will be cleared.
void GFX_EXPORT ParseFontLinkEntry(const std::string& entry,
                                   std::string* filename,
                                   std::string* font_name);

// Parses a font |family| in the format "FamilyFoo & FamilyBar (TrueType)".
// Splits by '&' and strips off the trailing parenthesized expression.
void GFX_EXPORT ParseFontFamilyString(const std::string& family,
                                      std::vector<std::string>* font_names);

}  // namespace internal

}  // namespace gfx

#endif  // UI_GFX_FONT_FALLBACK_WIN_H_
