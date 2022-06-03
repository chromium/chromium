// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/axis_transform2d.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace gfx {
namespace {

TEST(AxisTransform2dTest, Mapping) {
  AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));

  PointF p(150.f, 100.f);
  EXPECT_EQ(PointF(191.25f, 180.f), t.MapPoint(p));
  EXPECT_POINTF_EQ(PointF(117.f, 36.f), t.InverseMapPoint(p));

  RectF r(150.f, 100.f, 22.5f, 37.5f);
  EXPECT_EQ(RectF(191.25f, 180.f, 28.125f, 46.875f), t.MapRect(r));
  EXPECT_RECTF_EQ(RectF(117.f, 36.f, 18.f, 30.f), t.InverseMapRect(r));
}

TEST(AxisTransform2dTest, Scaling) {
  {
    AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));
    EXPECT_EQ(AxisTransform2d(1.5625f, Vector2dF(3.75f, 55.f)),
              PreScaleAxisTransform2d(t, 1.25));
    t.PreScale(Vector2dF(1.25f, 1.25f));
    EXPECT_EQ(AxisTransform2d(1.5625f, Vector2dF(3.75f, 55.f)), t);
  }

  {
    AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));
    EXPECT_EQ(AxisTransform2d(1.5625f, Vector2dF(4.6875f, 68.75f)),
              PostScaleAxisTransform2d(t, 1.25));
    t.PostScale(Vector2dF(1.25f, 1.25f));
    EXPECT_EQ(AxisTransform2d(1.5625f, Vector2dF(4.6875f, 68.75f)), t);
  }
}

TEST(AxisTransform2dTest, Translating) {
  Vector2dF tr(3.f, -5.f);
  {
    AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));
    EXPECT_EQ(AxisTransform2d(1.25f, Vector2dF(7.5f, 48.75f)),
              PreTranslateAxisTransform2d(t, tr));
    t.PreTranslate(tr);
    EXPECT_EQ(AxisTransform2d(1.25f, Vector2dF(7.5f, 48.75f)), t);
  }

  {
    AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));
    EXPECT_EQ(AxisTransform2d(1.25f, Vector2dF(6.75f, 50.f)),
              PostTranslateAxisTransform2d(t, tr));
    t.PostTranslate(tr);
    EXPECT_EQ(AxisTransform2d(1.25f, Vector2dF(6.75f, 50.f)), t);
  }
}

TEST(AxisTransform2dTest, Concat) {
  AxisTransform2d pre(1.25f, Vector2dF(3.75f, 55.f));
  AxisTransform2d post(0.5f, Vector2dF(10.f, 5.f));
  AxisTransform2d expectation(0.625f, Vector2dF(11.875f, 32.5f));
  EXPECT_EQ(expectation, ConcatAxisTransform2d(post, pre));

  AxisTransform2d post_concat = pre;
  post_concat.PostConcat(post);
  EXPECT_EQ(expectation, post_concat);

  AxisTransform2d pre_concat = post;
  pre_concat.PreConcat(pre);
  EXPECT_EQ(expectation, pre_concat);
}

TEST(AxisTransform2dTest, Inverse) {
  AxisTransform2d t(1.25f, Vector2dF(3.75f, 55.f));
  AxisTransform2d inv_inplace = t;
  inv_inplace.Invert();
  AxisTransform2d inv_out_of_place = InvertAxisTransform2d(t);

  EXPECT_AXIS_TRANSFORM2D_EQ(inv_inplace, inv_out_of_place);
  EXPECT_AXIS_TRANSFORM2D_EQ(AxisTransform2d(),
                             ConcatAxisTransform2d(t, inv_inplace));
  EXPECT_AXIS_TRANSFORM2D_EQ(AxisTransform2d(),
                             ConcatAxisTransform2d(inv_inplace, t));
}

}  // namespace
}  // namespace gfx
