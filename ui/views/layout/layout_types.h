// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
#define UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_

#include <algorithm>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"

namespace views {

// Describes how elements should be aligned within a layout.  Baseline alignment
// only makes sense on the vertical axis and is unsupported by most layout
// managers.
enum class LayoutAlignment { kStart, kCenter, kEnd, kStretch, kBaseline };

// Whether a layout is oriented horizontally or vertically.
enum class LayoutOrientation {
  kHorizontal,
  kVertical,
};

// A value used to bound a View during layout.  May be a finite bound or
// "unbounded", which is treated as larger than any finite value.
class VIEWS_EXPORT SizeBound {
 public:
  // Method definitions below to avoid "complex constructor" warning.  Marked
  // explicitly inline because Clang currently doesn't realize that "constexpr"
  // explicitly means "inline" and thus should count as "intentionally inlined
  // and thus shouldn't be warned about".
  // TODO(crbug.com/40116092): Remove "inline" if Clang's isInlineSpecified()
  // learns about constexpr.
  // TODO(crbug.com/40116093): Put method bodies here if complex constructor
  // heuristic learns to peer into types to discover that e.g. Optional is not
  // complex.
  inline constexpr SizeBound();
  inline constexpr SizeBound(int bound);  // NOLINT
  inline constexpr SizeBound(const SizeBound&);
  inline constexpr SizeBound(SizeBound&&);
  SizeBound& operator=(const SizeBound&) = default;
  SizeBound& operator=(SizeBound&&) = default;
  ~SizeBound() = default;

  constexpr bool is_bounded() const { return bound_.has_value(); }
  // Must only be invoked when there is a bound, since otherwise the value to
  // return is unknown.
  constexpr int value() const {
    DCHECK(is_bounded());
    return *bound_;
  }

  constexpr int min_of(int value) const {
    return is_bounded() ? std::min(this->value(), value) : value;
  }

  constexpr int value_or(int defaule_value) const {
    return is_bounded() ? value() : defaule_value;
  }

  void operator+=(const SizeBound& rhs);
  void operator-=(const SizeBound& rhs);

  std::string ToString() const;

 private:
  friend constexpr bool operator==(const SizeBound& lhs, const SizeBound& rhs);
  friend constexpr bool operator!=(const SizeBound& lhs, const SizeBound& rhs);

  // nullopt represents "unbounded".
  std::optional<int> bound_;
};
constexpr SizeBound::SizeBound() = default;
constexpr SizeBound::SizeBound(int bound) : bound_(bound) {}
constexpr SizeBound::SizeBound(const SizeBound&) = default;
constexpr SizeBound::SizeBound(SizeBound&&) = default;
VIEWS_EXPORT SizeBound operator+(const SizeBound& lhs, const SizeBound& rhs);
VIEWS_EXPORT SizeBound operator-(const SizeBound& lhs, const SizeBound& rhs);
// Note: These comparisons treat unspecified values similar to infinity, that
// is, larger than any specified value and equal to any other unspecified value.
// While one can argue that two unspecified values can't be compared (and thus
// all comparisons should return false), this isn't what any callers want and
// breaks things in subtle ways, e.g. "a = b; DCHECK(a == b)" may fail.
constexpr bool operator<(const SizeBound& lhs, const SizeBound& rhs) {
  return lhs.is_bounded() && (!rhs.is_bounded() || (lhs.value() < rhs.value()));
}
constexpr bool operator>(const SizeBound& lhs, const SizeBound& rhs) {
  return rhs.is_bounded() && (!lhs.is_bounded() || (lhs.value() > rhs.value()));
}
constexpr bool operator<=(const SizeBound& lhs, const SizeBound& rhs) {
  return !(lhs > rhs);
}
constexpr bool operator>=(const SizeBound& lhs, const SizeBound& rhs) {
  return !(lhs < rhs);
}
constexpr bool operator==(const SizeBound& lhs, const SizeBound& rhs) {
  return lhs.bound_ == rhs.bound_;
}
constexpr bool operator!=(const SizeBound& lhs, const SizeBound& rhs) {
  return lhs.bound_ != rhs.bound_;
}

// Stores an optional width and height upper bound. Used when calculating the
// preferred size of a layout pursuant to a maximum available size.
class VIEWS_EXPORT SizeBounds {
 public:
  // See comments in SizeBound re: "inline" with definitions below.
  inline constexpr SizeBounds();
  inline constexpr SizeBounds(SizeBound width, SizeBound height);
  inline constexpr explicit SizeBounds(const gfx::Size& size);
  inline constexpr SizeBounds(const SizeBounds&);
  inline constexpr SizeBounds(SizeBounds&&);
  SizeBounds& operator=(const SizeBounds&) = default;
  SizeBounds& operator=(SizeBounds&&) = default;
  ~SizeBounds() = default;

  constexpr const SizeBound& width() const { return width_; }
  constexpr SizeBound& width() { return width_; }
  void set_width(SizeBound width) { width_ = std::move(width); }

  constexpr const SizeBound& height() const { return height_; }
  constexpr SizeBound& height() { return height_; }
  void set_height(SizeBound height) { height_ = std::move(height); }

  constexpr bool is_fully_bounded() const {
    return width_.is_bounded() && height_.is_bounded();
  }

  // Enlarges (or shrinks, if negative) each upper bound that is present by the
  // specified amounts.
  void Enlarge(int width, int height);

  // Shrink the SizeBounds by the given `insets`.
  SizeBounds Inset(const gfx::Insets& inset) const;

  std::string ToString() const;

 private:
  SizeBound width_;
  SizeBound height_;
};
constexpr SizeBounds::SizeBounds() = default;
constexpr SizeBounds::SizeBounds(SizeBound width, SizeBound height)
    : width_(std::move(width)), height_(std::move(height)) {}
constexpr SizeBounds::SizeBounds(const gfx::Size& size)
    : width_(size.width()), height_(size.height()) {}
constexpr SizeBounds::SizeBounds(const SizeBounds&) = default;
constexpr SizeBounds::SizeBounds(SizeBounds&&) = default;
constexpr bool operator==(const SizeBounds& lhs, const SizeBounds& rhs) {
  return std::tie(lhs.width(), lhs.height()) ==
         std::tie(rhs.width(), rhs.height());
}
constexpr bool operator!=(const SizeBounds& lhs, const SizeBounds& rhs) {
  return !(lhs == rhs);
}
constexpr bool operator<(const SizeBounds& lhs, const SizeBounds& rhs) {
  return std::tie(lhs.height(), lhs.width()) <
         std::tie(rhs.height(), rhs.width());
}

// Returns true if the specified |size| can fit in the specified |bounds|.
// Returns false if either the width or height of |bounds| is specified and is
// smaller than the corresponding element of |size|.
VIEWS_EXPORT bool CanFitInBounds(const gfx::Size& size,
                                 const SizeBounds& bounds);

// These are declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(const SizeBounds& size_bounds, ::std::ostream* os);
void PrintTo(LayoutOrientation layout_orientation, ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
