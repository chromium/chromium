// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RANGE_RANGE_F_H_
#define UI_GFX_RANGE_RANGE_F_H_

#include <limits>
#include <ostream>
#include <string>

#include "ui/gfx/range/gfx_range_export.h"
#include "ui/gfx/range/range.h"

namespace gfx {

// A float version of Range. RangeF is made of a start and end position; when
// they are the same, the range is empty. Note that |start_| can be greater
// than |end_| to respect the directionality of the range.
class GFX_RANGE_EXPORT RangeF {
 public:
  // Creates an empty range {0,0}.
  constexpr RangeF() : RangeF(0.f) {}

  // Initializes the range with a start and end.
  constexpr RangeF(float start, float end) : start_(start), end_(end) {}

  // Initializes the range with the same start and end positions.
  constexpr explicit RangeF(float position) : RangeF(position, position) {}

  // Returns a range that is invalid, which is {float_max,float_max}.
  static constexpr RangeF InvalidRange() {
    return RangeF(std::numeric_limits<float>::max());
  }

  // Checks if the range is valid through comparison to InvalidRange().
  constexpr bool IsValid() const { return *this != InvalidRange(); }

  // Getters and setters.
  constexpr float start() const { return start_; }
  void set_start(float start) { start_ = start; }

  constexpr float end() const { return end_; }
  void set_end(float end) { end_ = end; }

  // Returns the absolute value of the length.
  constexpr float length() const { return GetMax() - GetMin(); }

  constexpr bool is_reversed() const { return start() > end(); }
  constexpr bool is_empty() const { return start() == end(); }

  // Returns the minimum and maximum values.
  constexpr float GetMin() const { return start() < end() ? start() : end(); }
  constexpr float GetMax() const { return start() > end() ? start() : end(); }

  constexpr bool operator==(const RangeF& other) const {
    return start() == other.start() && end() == other.end();
  }
  constexpr bool operator!=(const RangeF& other) const {
    return !(*this == other);
  }
  constexpr bool EqualsIgnoringDirection(const RangeF& other) const {
    return GetMin() == other.GetMin() && GetMax() == other.GetMax();
  }

  // Returns true if this range intersects the specified |range|.
  constexpr bool Intersects(const RangeF& range) const {
    return Intersect(range).IsValid();
  }

  // Returns true if this range is contained by the specified |range| or it is
  // an empty range and ending the range |range|. (copied from gfx::Range)
  constexpr bool IsBoundedBy(const RangeF& range) const {
    return IsValid() && range.IsValid() && GetMin() >= range.GetMin() &&
           GetMax() <= range.GetMax();
  }

  // Returns true if this range contains the specified |range|.
  constexpr bool Contains(const RangeF& range) const {
    return range.IsBoundedBy(*this) &&
           // A non-empty range doesn't contain the range [max, max).
           (range.GetMax() != GetMax() || range.is_empty() == is_empty());
  }

  // Computes the intersection of this range with the given |range|.
  // If they don't intersect, it returns an InvalidRange().
  // The returned range is always empty or forward (never reversed).
  constexpr RangeF Intersect(const RangeF& range) const {
    const float min = std::max(GetMin(), range.GetMin());
    const float max = std::min(GetMax(), range.GetMax());

    return (min < max || Contains(range) || range.Contains(*this))
               ? RangeF(min, max)
               : InvalidRange();
  }

  RangeF Intersect(const Range& range) const;

  // Floor/Ceil/Round the start and end values of the given RangeF.
  Range Floor() const;
  Range Ceil() const;
  Range Round() const;

  std::string ToString() const;

 private:
  float start_;
  float end_;
};

GFX_RANGE_EXPORT std::ostream& operator<<(std::ostream& os,
                                          const RangeF& range);

}  // namespace gfx

#endif  // UI_GFX_RANGE_RANGE_F_H_
