// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_
#define UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_

#include <tuple>

#include "third_party/skia/include/core/SkColor.h"

namespace ui::decoration {

// Describes a highlight border: two concentric rounded-rectangle rings around a
// piece of content. Depending on `insets_type()`, the outer ring
// (`border_color`) and the inner ring (`highlight_color`) sit inside, straddle,
// or sit outside the content edge, giving the content contrast against both
// light and dark backgrounds.
//
// This is a pure value type with no dependency on layers or caching, and it
// serves as the cache key for highlight border ninebox images.
class HighlightBorderValue {
 public:
  // How the highlight border is positioned relative to the content bounds.
  enum class InsetsType {
    // Both the outer and inner rings sit inside the content bounds, overlapping
    // the outer 2 * `thickness()` of the content.
    kNoInsets,
    // The outer ring sits outside the content bounds and the inner ring sits
    // inside the content bounds, overlapping the outer `thickness()` of the
    // content.
    kHalfInsets,
    // Both the outer and inner rings sit outside the content bounds, so there
    // is no overlap with the content.
    kFullInsets,
  };

  constexpr HighlightBorderValue(SkColor highlight_color,
                                 SkColor border_color,
                                 int thickness = 1,
                                 bool is_pill_shaped = false,
                                 InsetsType insets_type = InsetsType::kNoInsets)
      : highlight_color_(highlight_color),
        border_color_(border_color),
        thickness_(thickness),
        is_pill_shaped_(is_pill_shaped),
        insets_type_(insets_type) {}

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

  constexpr InsetsType insets_type() const { return insets_type_; }

  // Total thickness of the rings that sit outside the content bounds.
  constexpr int GetOutwardThickness() const {
    switch (insets_type_) {
      case InsetsType::kNoInsets:
        return 0;
      case InsetsType::kHalfInsets:
        return thickness_;
      case InsetsType::kFullInsets:
        return 2 * thickness_;
    }
  }

  // Total thickness of the rings that sit inside the content bounds.
  constexpr int GetInwardThickness() const {
    return 2 * thickness_ - GetOutwardThickness();
  }

  friend constexpr bool operator==(const HighlightBorderValue&,
                                   const HighlightBorderValue&) = default;

  // Strict weak ordering so that the value can be used as a cache key.
  friend constexpr bool operator<(const HighlightBorderValue& lhs,
                                  const HighlightBorderValue& rhs) {
    return std::tie(lhs.highlight_color_, lhs.border_color_, lhs.thickness_,
                    lhs.is_pill_shaped_, lhs.insets_type_) <
           std::tie(rhs.highlight_color_, rhs.border_color_, rhs.thickness_,
                    rhs.is_pill_shaped_, rhs.insets_type_);
  }

 private:
  SkColor highlight_color_;
  SkColor border_color_;
  int thickness_;
  bool is_pill_shaped_ = false;
  InsetsType insets_type_ = InsetsType::kNoInsets;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_HIGHLIGHT_BORDER_VALUE_H_
