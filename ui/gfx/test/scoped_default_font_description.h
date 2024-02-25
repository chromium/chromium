// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_SCOPED_DEFAULT_FONT_DESCRIPTION_H_
#define UI_GFX_TEST_SCOPED_DEFAULT_FONT_DESCRIPTION_H_

#include "ui/gfx/font_list.h"

namespace gfx {

// Tests should use this scoped setter, instead of calling
// SetDefaultFontDescription directly.
class ScopedDefaultFontDescription {
 public:
  explicit ScopedDefaultFontDescription(const std::string& font_description) {
    FontList::SetDefaultFontDescription(font_description);
  }
  ~ScopedDefaultFontDescription() {
    FontList::SetDefaultFontDescription(std::string());
  }
};

}  // namespace gfx

#endif  // UI_GFX_TEST_SCOPED_DEFAULT_FONT_DESCRIPTION_H_
