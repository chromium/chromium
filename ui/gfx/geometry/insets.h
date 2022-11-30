// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_H_
#define UI_GFX_GEOMETRY_INSETS_H_

#include "base/numerics/clamped_math.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/insets_outsets_base.h"

namespace gfx {

class Outsets;
class Vector2d;

// This can be used to represent a space within a rectangle, by "shrinking" the
// rectangle by the inset amount on all four sides. Alternatively, it can
// represent a border that has a different thickness on each side.
class GEOMETRY_EXPORT Insets : public InsetsOutsetsBase<Insets> {
 public:
  using InsetsOutsetsBase::InsetsOutsetsBase;

  // Conversion from Insets to Outsets negates all components.
  Outsets ToOutsets() const;

  // Adjusts the vertical and horizontal dimensions by the values described in
  // |vector|. Offsetting insets before applying to a rectangle would be
  // equivalent to offsetting the rectangle then applying the insets.
  void Offset(const gfx::Vector2d& vector);

  explicit operator InsetsF() const {
    return InsetsF()
        .set_top(static_cast<float>(top()))
        .set_left(static_cast<float>(left()))
        .set_bottom(static_cast<float>(bottom()))
        .set_right(static_cast<float>(right()));
  }
};

inline Insets operator+(Insets lhs, const Insets& rhs) {
  lhs += rhs;
  return lhs;
}

inline Insets operator-(Insets lhs, const Insets& rhs) {
  lhs -= rhs;
  return lhs;
}

inline Insets operator+(Insets insets, const gfx::Vector2d& offset) {
  insets.Offset(offset);
  return insets;
}

// Helper methods to scale a gfx::Insets to a new gfx::Insets.
GEOMETRY_EXPORT Insets ScaleToCeiledInsets(const Insets& insets,
                                           float x_scale,
                                           float y_scale);
GEOMETRY_EXPORT Insets ScaleToCeiledInsets(const Insets& insets, float scale);
GEOMETRY_EXPORT Insets ScaleToFlooredInsets(const Insets& insets,
                                            float x_scale,
                                            float y_scale);
GEOMETRY_EXPORT Insets ScaleToFlooredInsets(const Insets& insets, float scale);
GEOMETRY_EXPORT Insets ScaleToRoundedInsets(const Insets& insets,
                                            float x_scale,
                                            float y_scale);
GEOMETRY_EXPORT Insets ScaleToRoundedInsets(const Insets& insets, float scale);

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Insets&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_H_
