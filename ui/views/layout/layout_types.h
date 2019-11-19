// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
#define UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_

#include <string>

#include "base/optional.h"
#include "ui/views/views_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace views {

// Whether a layout is oriented horizontally or vertically.
enum class LayoutOrientation {
  kHorizontal,
  kVertical,
};

// Stores an optional width and height upper bound. Used when calculating the
// preferred size of a layout pursuant to a maximum available size.
class VIEWS_EXPORT SizeBounds {
 public:
  SizeBounds();
  SizeBounds(const base::Optional<int>& width,
             const base::Optional<int>& height);
  explicit SizeBounds(const gfx::Size& size);
  SizeBounds(const SizeBounds& other);

  const base::Optional<int>& width() const { return width_; }
  void set_width(const base::Optional<int>& width) { width_ = width; }

  const base::Optional<int>& height() const { return height_; }
  void set_height(const base::Optional<int>& height) { height_ = height; }

  // Enlarges (or shrinks, if negative) each upper bound that is present by the
  // specified amounts.
  void Enlarge(int width, int height);

  bool operator==(const SizeBounds& other) const;
  bool operator!=(const SizeBounds& other) const;
  bool operator<(const SizeBounds& other) const;

  std::string ToString() const;

 private:
  base::Optional<int> width_;
  base::Optional<int> height_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
