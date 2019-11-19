// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_types.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/size.h"

namespace views {

// SizeBounds ------------------------------------------------------------------

SizeBounds::SizeBounds() = default;

SizeBounds::SizeBounds(const base::Optional<int>& width,
                       const base::Optional<int>& height)
    : width_(width), height_(height) {}

SizeBounds::SizeBounds(const SizeBounds& other)
    : width_(other.width()), height_(other.height()) {}

SizeBounds::SizeBounds(const gfx::Size& other)
    : width_(other.width()), height_(other.height()) {}

void SizeBounds::Enlarge(int width, int height) {
  if (width_)
    width_ = std::max(0, *width_ + width);
  if (height_)
    height_ = std::max(0, *height_ + height);
}

bool SizeBounds::operator==(const SizeBounds& other) const {
  return width_ == other.width_ && height_ == other.height_;
}

bool SizeBounds::operator!=(const SizeBounds& other) const {
  return !(*this == other);
}

bool SizeBounds::operator<(const SizeBounds& other) const {
  return std::tie(height_, width_) < std::tie(other.height_, other.width_);
}

std::string SizeBounds::ToString() const {
  std::ostringstream oss;
  if (width().has_value())
    oss << *width();
  else
    oss << "_";
  oss << " x ";
  if (height().has_value())
    oss << *height();
  else
    oss << "_";
  return oss.str();
}

}  // namespace views
