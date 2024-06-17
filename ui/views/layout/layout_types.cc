// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_types.h"

#include <algorithm>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace views {

// SizeBound -------------------------------------------------------------------

void SizeBound::operator+=(const SizeBound& rhs) {
  if (!rhs.is_bounded())
    bound_.reset();
  else if (is_bounded())
    *bound_ += rhs.value();
}

void SizeBound::operator-=(const SizeBound& rhs) {
  if (!rhs.is_bounded())
    bound_ = 0;
  else if (is_bounded())
    *bound_ -= rhs.value();
}

std::string SizeBound::ToString() const {
  return is_bounded() ? base::NumberToString(*bound_) : "_";
}

SizeBound operator+(const SizeBound& lhs, const SizeBound& rhs) {
  SizeBound result = lhs;
  result += rhs;
  return result;
}

SizeBound operator-(const SizeBound& lhs, const SizeBound& rhs) {
  SizeBound result = lhs;
  result -= rhs;
  return result;
}

// SizeBounds ------------------------------------------------------------------

void SizeBounds::Enlarge(int width, int height) {
  width_ = std::max<SizeBound>(0, width_ + width);
  height_ = std::max<SizeBound>(0, height_ + height);
}

SizeBounds SizeBounds::Inset(const gfx::Insets& inset) const {
  SizeBounds new_size_bounds(*this);
  new_size_bounds.Enlarge(-inset.width(), -inset.height());
  return new_size_bounds;
}

std::string SizeBounds::ToString() const {
  return base::StrCat({width_.ToString(), " x ", height_.ToString()});
}

bool CanFitInBounds(const gfx::Size& size, const SizeBounds& bounds) {
  return bounds.width() >= size.width() && bounds.height() >= size.height();
}

}  // namespace views
