// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#include "ui/gfx/canvas.h"

namespace gfx {

int GetStringWidth(const std::u16string& text, const FontList& font_list) {
  return Canvas::GetStringWidth(text, font_list);
}

float GetStringWidthF(const std::u16string& text, const FontList& font_list) {
  return Canvas::GetStringWidthF(text, font_list);
}

}  // namespace gfx
