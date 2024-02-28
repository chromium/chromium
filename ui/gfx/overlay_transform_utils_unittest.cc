// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_transform_utils.h"

#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace {

struct RectInViewport {
  Rect rect;
  Size viewport;
};

RectInViewport ApplyOverlayTransform(OverlayTransform overlay_transform,
                                     const RectInViewport& original) {
  auto transform =
      OverlayTransformToTransform(overlay_transform, SizeF(original.viewport));
  RectInViewport result;
  result.rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
      transform, original.rect);
  result.viewport = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                        transform, Rect(original.viewport))
                        .size();
  return result;
}

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
      {OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90, Rect(90, 10, 100, 50)},
      {OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180, Rect(40, 90, 50, 100)},
      {OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270, Rect(10, 40, 100, 50)},
      {OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90, Rect(10, 10, 100, 50)},
      {OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270, Rect(90, 40, 100, 50)},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(static_cast<int>(test_case.overlay_transform));

    RectInViewport transformed = ApplyOverlayTransform(
        test_case.overlay_transform, {original, viewport_bounds});
    EXPECT_EQ(test_case.transformed, transformed.rect);

    RectInViewport inverted = ApplyOverlayTransform(
        InvertOverlayTransform(test_case.overlay_transform),
        {test_case.transformed, transformed.viewport});
    EXPECT_EQ(original, inverted.rect);
  }
}

TEST(OverlayTransformUtilTest, Concat) {
  const std::vector<OverlayTransform> kTransforms = {
      OVERLAY_TRANSFORM_INVALID,
      OVERLAY_TRANSFORM_NONE,
      OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
      OVERLAY_TRANSFORM_FLIP_VERTICAL,
      OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
      OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
      OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
      OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90,
      OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270,
  };

  for (auto t1 : kTransforms) {
    SCOPED_TRACE(static_cast<int>(t1));
    for (auto t2 : kTransforms) {
      SCOPED_TRACE(static_cast<int>(t2));

      OverlayTransform concat_transform = OverlayTransformsConcat(t1, t2);
      if (t1 == OVERLAY_TRANSFORM_INVALID || t2 == OVERLAY_TRANSFORM_INVALID) {
        EXPECT_EQ(concat_transform, OVERLAY_TRANSFORM_INVALID);
        continue;
      }
      EXPECT_NE(concat_transform, OVERLAY_TRANSFORM_INVALID);

      RectInViewport original{
          .rect = Rect(10, 10, 50, 100),
          .viewport = Size(100, 200),
      };

      RectInViewport t1_then_t2_result =
          ApplyOverlayTransform(t2, ApplyOverlayTransform(t1, original));
      RectInViewport concat_result =
          ApplyOverlayTransform(concat_transform, original);

      EXPECT_EQ(t1_then_t2_result.rect, concat_result.rect);
      EXPECT_EQ(t1_then_t2_result.viewport, concat_result.viewport);
    }
  }
}

}  // namespace
}  // namespace gfx
