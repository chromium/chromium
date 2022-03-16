// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_F_H_
#define UI_GFX_GEOMETRY_INSETS_F_H_

#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/insets_outsets_f_base.h"

namespace gfx {

class OutsetsF;

// A floating point version of gfx::Insets.
class GEOMETRY_EXPORT InsetsF : public InsetsOutsetsFBase<InsetsF> {
 public:
  using InsetsOutsetsFBase::InsetsOutsetsFBase;

  // Avoid this constructor in blink code because it's easy to make mistakes in
  // the order of the parameters. Use the other constructors and set_*()
  // methods instead.
  constexpr InsetsF(float vertical, float horizontal)
      : InsetsF(vertical, horizontal, vertical, horizontal) {}

  // Avoid this constructor in blink code because it's easy to make mistakes in
  // the order of the parameters. Use the other constructors and set_*()
  // methods instead.
  constexpr InsetsF(float top, float left, float bottom, float right) {
    set_top(top);
    set_left(left);
    set_bottom(bottom);
    set_right(right);
  }

  // Conversion from InsetsF to OutsetsF negates all components.
  OutsetsF ToOutsets() const;

  // Avoid this method in blink code because it's easy to make mistakes in the
  // order of the parameters. Use the setter methods instead.
  void Set(float top, float left, float bottom, float right) {
    set_top(top);
    set_left(left);
    set_bottom(bottom);
    set_right(right);
  }
};

inline InsetsF ScaleInsets(InsetsF i, float x_scale, float y_scale) {
  i.Scale(x_scale, y_scale);
  return i;
}

inline InsetsF ScaleInsets(const InsetsF& i, float scale) {
  return ScaleInsets(i, scale, scale);
}

inline InsetsF operator+(InsetsF lhs, const InsetsF& rhs) {
  lhs += rhs;
  return lhs;
}

inline InsetsF operator-(InsetsF lhs, const InsetsF& rhs) {
  lhs -= rhs;
  return lhs;
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const InsetsF&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_F_H_
