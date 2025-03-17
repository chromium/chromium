// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ui/gfx/canvas.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

int GetStringWidth(std::u16string_view text, const FontList& font_list) {
  return Canvas::GetStringWidth(text, font_list);
}

float GetStringWidthF(std::u16string_view text, const FontList& font_list) {
  return Canvas::GetStringWidthF(text, font_list);
}

}  // namespace gfx
