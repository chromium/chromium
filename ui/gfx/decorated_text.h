// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DECORATED_TEXT_H_
#define UI_GFX_DECORATED_TEXT_H_

#include <string>
#include <vector>

#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/range/range.h"

namespace gfx {

// Encapsulates styling information for some given text.
struct GFX_EXPORT DecoratedText {
  // Describes the various text decoration attributes applicable to a given
  // range of text.
  struct GFX_EXPORT RangedAttribute {
    // Disallow default construction of Font, since that's slow.
    RangedAttribute() = delete;
    RangedAttribute(const Range& range, const Font& font);

    // The range in |text|, this RangedAttribute corresponds to. Should not be
    // reversed and should lie within the bounds of |text|.
    Range range;
    Font font;
    bool strike;
  };

  DecoratedText();
  ~DecoratedText();

  std::u16string text;

  // Vector of RangedAttribute describing styling of non-overlapping ranges
  // in |text|.
  std::vector<RangedAttribute> attributes;
};

}  // namespace gfx

#endif  // UI_GFX_DECORATED_TEXT_H_
