// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_OUTSETS_H_
#define UI_GFX_GEOMETRY_OUTSETS_H_

#include "base/numerics/clamped_math.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"

namespace gfx {

// This can be used to represent a space surrounding a rectangle, by
// "expanding" the rectangle by the outset amount on all four sides.
class Outsets : public InsetsOutsetsBase<Outsets> {
 public:
  using InsetsOutsetsBase::InsetsOutsetsBase;

  // Conversion from Outsets to Insets negates all components.
  Insets ToInsets() const {
    return Insets()
        .set_left_right(-left(), -right())
        .set_top_bottom(-top(), -bottom());
  }
};

inline Outsets operator+(Outsets lhs, const Outsets& rhs) {
  lhs += rhs;
  return lhs;
}

inline Outsets operator-(Outsets lhs, const Outsets& rhs) {
  lhs -= rhs;
  return lhs;
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Outsets&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_OUTSETS_H_
