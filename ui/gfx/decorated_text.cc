// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/decorated_text.h"

namespace gfx {

DecoratedText::RangedAttribute::RangedAttribute(const gfx::Range& range,
                                                const gfx::Font& font)
    : range(range), font(font), strike(false) {}

DecoratedText::DecoratedText() {}

DecoratedText::~DecoratedText() {}

}  // namespace gfx
