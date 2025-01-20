// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_
#define UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_

#include <array>

#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "ui/gfx/geometry/quaternion.h"

namespace gfx {

// Contains the components of a factored transform. These components may be
// blended and recomposed.
struct COMPONENT_EXPORT(GEOMETRY) DecomposedTransform {
  // The default constructor initializes the components in such a way that
  // will compose the identity transform.
  std::array<double, 3u> translate = {0, 0, 0};
  std::array<double, 3u> scale = {1, 1, 1};
  std::array<double, 3u> skew = {0, 0, 0};
  std::array<double, 4u> perspective = {0, 0, 0, 1};
  Quaternion quaternion;

  std::string ToString() const;
};

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const DecomposedTransform&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_DECOMPOSED_TRANSFORM_H_
