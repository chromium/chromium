// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_OUTSETS_F_H_
#define UI_GFX_GEOMETRY_OUTSETS_F_H_

#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/insets_outsets_f_base.h"

namespace gfx {

// A floating point version of gfx::Outsets.
class GEOMETRY_EXPORT OutsetsF : public InsetsOutsetsFBase<OutsetsF> {
 public:
  using InsetsOutsetsFBase::InsetsOutsetsFBase;

  // Conversion from OutsetsF to InsetsF negates all components.
  InsetsF ToInsets() const {
    return InsetsF()
        .set_left(-left())
        .set_right(-right())
        .set_top(-top())
        .set_bottom(-bottom());
  }
};

inline OutsetsF operator+(OutsetsF lhs, const OutsetsF& rhs) {
  lhs += rhs;
  return lhs;
}

inline OutsetsF operator-(OutsetsF lhs, const OutsetsF& rhs) {
  lhs -= rhs;
  return lhs;
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const OutsetsF&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_OUTSETS_F_H_
