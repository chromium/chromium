// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/badge.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/label.h"

namespace views {

Badge::Badge(const std::u16string& text) : text_(text) {}

Badge::~Badge() = default;

const std::u16string& Badge::GetText() const {
  return text_;
}

void Badge::SetText(const std::u16string& text) {
  text_ = text;

  OnPropertyChanged(&text_, kPropertyEffectsPreferredSizeChanged);
}

gfx::Size Badge::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return BadgePainter::GetBadgeSize(text_, Label::GetDefaultFontList());
}

void Badge::OnPaint(gfx::Canvas* canvas) {
  BadgePainter::PaintBadge(canvas, this, 0, 0, text_,
                           Label::GetDefaultFontList());
}

BEGIN_METADATA(Badge)
ADD_PROPERTY_METADATA(std::u16string, Text)
END_METADATA

}  // namespace views
