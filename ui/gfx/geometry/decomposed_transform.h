// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_
#define UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_

#include "base/dcheck_is_on.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/quaternion.h"

namespace gfx {

// Contains the components of a factored transform. These components may be
// blended and recomposed.
struct GEOMETRY_EXPORT DecomposedTransform {
  // The default constructor initializes the components in such a way that
  // will compose the identity transform.
  double translate[3] = {0, 0, 0};
  double scale[3] = {1, 1, 1};
  double skew[3] = {0, 0, 0};
  double perspective[4] = {0, 0, 0, 1};
  Quaternion quaternion;

  std::string ToString() const;
};

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const DecomposedTransform&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_
