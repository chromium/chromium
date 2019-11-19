// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"

namespace views {

Separator::Separator() = default;

Separator::~Separator() = default;

SkColor Separator::GetColor() const {
  if (overridden_color_ == true)
    return overridden_color_.value();
  return 0;
}

void Separator::SetColor(SkColor color) {
  if (overridden_color_ == color)
    return;

  overridden_color_ = color;
  OnPropertyChanged(&overridden_color_, kPropertyEffectsPaint);
}

int Separator::GetPreferredHeight() const {
  return preferred_height_;
}

void Separator::SetPreferredHeight(int height) {
  if (preferred_height_ == height)
    return;

  preferred_height_ = height;
  OnPropertyChanged(&preferred_height_, kPropertyEffectsPreferredSizeChanged);
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::CalculatePreferredSize() const {
  gfx::Size size(kThickness, preferred_height_);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void Separator::OnPaint(gfx::Canvas* canvas) {
  const SkColor color = overridden_color_
                            ? *overridden_color_
                            : GetNativeTheme()->GetSystemColor(
                                  ui::NativeTheme::kColorId_SeparatorColor);
  // Paint background and border, if any.
  View::OnPaint(canvas);

  gfx::ScopedCanvas scoped_canvas(canvas);
  const gfx::Insets insets = GetInsets();
  const gfx::Rect contents_bounds = GetContentsBounds();
  const float dsf = canvas->UndoDeviceScaleFactor();

  // This is a hybrid of gfx::ScaleToEnclosedRect() and
  // gfx::ScaleToEnclosingRect() based on whether there are nonzero insets on
  // any particular side of the separator. If there is no inset, the separator
  // will paint all the way out to the edge of the view. If there is an inset,
  // the extent of the separator will rounded inward so that it paints only
  // full pixels, providing a sharper appearance and preserving the inset.
  //
  // This allows separators that entirely fill their area to do so, and those
  // intended as spacers in a larger flow to do so as well. See
  // crbug.com/1016760 and crbug.com/1019503 for examples of why we need to
  // handle both cases.
  const int x = insets.left() == 0 ? std::floor(contents_bounds.x() * dsf)
                                   : std::ceil(contents_bounds.x() * dsf);
  const int y = insets.top() == 0 ? std::floor(contents_bounds.y() * dsf)
                                  : std::ceil(contents_bounds.y() * dsf);
  const int r = insets.right() == 0 ? std::ceil(contents_bounds.right() * dsf)
                                    : std::floor(contents_bounds.right() * dsf);
  const int b = insets.bottom() == 0
                    ? std::ceil(contents_bounds.bottom() * dsf)
                    : std::floor(contents_bounds.bottom() * dsf);

  // Minimum separator size is 1 px.
  const int w = std::max(1, r - x);
  const int h = std::max(1, b - y);

  canvas->FillRect({x, y, w, h}, color);
}

BEGIN_METADATA(Separator)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Separator, SkColor, Color)
ADD_PROPERTY_METADATA(Separator, int, PreferredHeight)
END_METADATA()

}  // namespace views
