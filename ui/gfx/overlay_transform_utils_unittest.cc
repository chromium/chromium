// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_transform_utils.h"

#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace {

TEST(OverlayTransformUtilTest, All) {
  const Size viewport_bounds(100, 200);
  const Rect original(10, 10, 50, 100);
  struct TestCase {
    OverlayTransform overlay_transform;
    Rect transformed;
  };

  TestCase test_cases[] = {
      {OVERLAY_TRANSFORM_NONE, Rect(10, 10, 50, 100)},
      {OVERLAY_TRANSFORM_FLIP_HORIZONTAL, Rect(40, 10, 50, 100)},
      {OVERLAY_TRANSFORM_FLIP_VERTICAL, Rect(10, 90, 50, 100)},
      {OVERLAY_TRANSFORM_ROTATE_90, Rect(90, 10, 100, 50)},
      {OVERLAY_TRANSFORM_ROTATE_180, Rect(40, 90, 50, 100)},
      {OVERLAY_TRANSFORM_ROTATE_270, Rect(10, 40, 100, 50)},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.overlay_transform);

    auto transform = OverlayTransformToTransform(test_case.overlay_transform,
                                                 gfx::SizeF(viewport_bounds));
    EXPECT_EQ(test_case.transformed,
              cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                  transform, original));

    auto transformed_viewport_size =
        cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
            transform, gfx::Rect(viewport_bounds))
            .size();
    auto inverse_transform = OverlayTransformToTransform(
        InvertOverlayTransform(test_case.overlay_transform),
        gfx::SizeF(transformed_viewport_size));
    EXPECT_EQ(original, cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                            inverse_transform, test_case.transformed));
  }
}

}  // namespace
}  // namespace gfx
