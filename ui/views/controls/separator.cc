// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include <algorithm>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/metadata/type_conversion.h"

namespace views {

constexpr int Separator::kThickness;

Separator::Separator() = default;

Separator::~Separator() = default;

ui::ColorId Separator::GetColorId() const {
  return color_id_;
}

void Separator::SetColorId(ui::ColorId color_id) {
  if (color_id_ == color_id) {
    return;
  }

  color_id_ = color_id;
  OnPropertyChanged(&color_id_, kPropertyEffectsPaint);
}

int Separator::GetPreferredLength() const {
  return preferred_length_;
}

void Separator::SetPreferredLength(int length) {
  if (preferred_length_ == length) {
    return;
  }

  preferred_length_ = length;
  OnPropertyChanged(&preferred_length_, kPropertyEffectsPreferredSizeChanged);
}

Separator::Orientation Separator::GetOrientation() const {
  return orientation_;
}

void Separator::SetOrientation(Orientation orientation) {
  orientation_ = orientation;
}

int Separator::GetBorderRadius() const {
  return border_radius_;
}

void Separator::SetBorderRadius(int radius) {
  border_radius_ = radius;
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  gfx::Size size(kThickness, preferred_length_);
  if (orientation_ == Orientation::kHorizontal) {
    size.Transpose();
  }

  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void Separator::OnPaint(gfx::Canvas* canvas) {
  const SkColor color = GetColorProvider()->GetColor(color_id_);
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

  if (border_radius_) {
    cc::PaintFlags flags;
    flags.setColor(color);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setBlendMode(SkBlendMode::kSrcOver);
    canvas->DrawRoundRect({x, y, w, h}, border_radius_, flags);
  } else {
    canvas->FillRect({x, y, w, h}, color);
  }
}

BEGIN_METADATA(Separator)
ADD_PROPERTY_METADATA(ui::ColorId, ColorId)
ADD_PROPERTY_METADATA(int, PreferredLength)
ADD_PROPERTY_METADATA(Separator::Orientation, Orientation)
ADD_PROPERTY_METADATA(int, BorderRadius)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(views::Separator::Orientation,
                       {views::Separator::Orientation::kHorizontal,
                        u"kHorizontal"},
                       {views::Separator::Orientation::kVertical, u"kVertical"})
