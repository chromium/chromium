// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_
#define UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_

#include <tuple>

#include "third_party/skia/include/core/SkColor.h"

namespace ui::decoration {

// Describes a highlight border: two concentric rounded-rectangle rings around a
// piece of content. The outer ring (`border_color`) and the inner ring
// (`highlight_color`) sit along the inside of the content edge, giving the
// content contrast against both light and dark backgrounds.
//
// This is a pure value type with no dependency on layers or caching, and it
// serves as the cache key for highlight border ninebox images.
class HighlightBorderValue {
 public:
  constexpr HighlightBorderValue(SkColor highlight_color,
                                 SkColor border_color,
                                 int thickness = 1,
                                 bool is_pill_shaped = false)
      : highlight_color_(highlight_color),
        border_color_(border_color),
        thickness_(thickness),
        is_pill_shaped_(is_pill_shaped) {}

  // Color of the inner ring.
  constexpr SkColor highlight_color() const { return highlight_color_; }

  // Color of the outer ring.
  constexpr SkColor border_color() const { return border_color_; }

  // Width of each ring. The rings are stroked `thickness` *physical pixels*
  // wide so that they stay crisp hairlines at any device scale factor (the same
  // behavior as views::HighlightBorder), while layout (the margin reserved
  // outside the content and the ninebox insets) treats it as DIPs.
  constexpr int thickness() const { return thickness_; }

  constexpr bool is_pill_shaped() const { return is_pill_shaped_; }

  friend constexpr bool operator==(const HighlightBorderValue&,
                                   const HighlightBorderValue&) = default;

  // Strict weak ordering so that the value can be used as a cache key.
  friend constexpr bool operator<(const HighlightBorderValue& lhs,
                                  const HighlightBorderValue& rhs) {
    return std::tie(lhs.highlight_color_, lhs.border_color_, lhs.thickness_,
                    lhs.is_pill_shaped_) <
           std::tie(rhs.highlight_color_, rhs.border_color_, rhs.thickness_,
                    rhs.is_pill_shaped_);
  }

 private:
  SkColor highlight_color_;
  SkColor border_color_;
  int thickness_;
  bool is_pill_shaped_ = false;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_
