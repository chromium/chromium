// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/axis_transform2d.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"

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

TEST(AxisTransform2dTest, ClampOutput) {
  double entries[][2] = {
      // The first entry is used to initialize the transform.
      // The second entry is used to initialize the object to be mapped.
      {std::numeric_limits<float>::max(),
       std::numeric_limits<float>::infinity()},
      {1, std::numeric_limits<float>::infinity()},
      {-1, std::numeric_limits<float>::infinity()},
      {1, -std::numeric_limits<float>::infinity()},
      {
          std::numeric_limits<float>::max(),
          std::numeric_limits<float>::max(),
      },
      {
          std::numeric_limits<float>::lowest(),
          -std::numeric_limits<float>::infinity(),
      },
  };

  for (double* entry : entries) {
    const float mv = entry[0];
    const float factor = entry[1];

    auto is_valid_point = [&](const PointF& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y());
    };
    auto is_valid_rect = [&](const RectF& r) -> bool {
      return is_valid_point(r.origin()) && std::isfinite(r.width()) &&
             std::isfinite(r.height());
    };

    auto test = [&](const AxisTransform2d& m) {
      SCOPED_TRACE(base::StringPrintf("m: %s factor: %lg", m.ToString().c_str(),
                                      factor));
      auto p = m.MapPoint(PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();

      // AxisTransform2d::MapRect() requires non-negative scales.
      if (m.scale().x() >= 0 && m.scale().y() >= 0) {
        auto r = m.MapRect(RectF(factor, factor, factor, factor));
        EXPECT_TRUE(is_valid_rect(r)) << r.ToString();
      }
    };

    test(AxisTransform2d::FromScaleAndTranslation(Vector2dF(mv, mv),
                                                  Vector2dF(mv, mv)));
    test(AxisTransform2d::FromScaleAndTranslation(Vector2dF(mv, mv),
                                                  Vector2dF(0, 0)));
    test(AxisTransform2d::FromScaleAndTranslation(Vector2dF(1, 1),
                                                  Vector2dF(mv, mv)));
  }
}

TEST(AxisTransform2dTest, Decompose) {
  {
    auto transform = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(2.5, -3.75), Vector2dF(4.25, -5.5));
    DecomposedTransform decomp = transform.Decompose();
    EXPECT_DECOMPOSED_TRANSFORM_EQ((DecomposedTransform{{4.25, -5.5, 0},
                                                        {2.5, -3.75, 1},
                                                        {0, 0, 0},
                                                        {0, 0, 0, 1},
                                                        {0, 0, 0, 1}}),
                                   decomp);
    EXPECT_EQ(Transform(transform), Transform::Compose(decomp));
  }
  {
    auto transform = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(-2.5, -3.75), Vector2dF(4.25, -5.5));
    DecomposedTransform decomp = transform.Decompose();
    EXPECT_DECOMPOSED_TRANSFORM_EQ((DecomposedTransform{{4.25, -5.5, 0},
                                                        {2.5, 3.75, 1},
                                                        {0, 0, 0},
                                                        {0, 0, 0, 1},
                                                        {0, 0, 1, 0}}),
                                   decomp);
    EXPECT_EQ(Transform(transform), Transform::Compose(decomp));
  }
  {
    auto transform =
        AxisTransform2d::FromScaleAndTranslation(Vector2dF(), Vector2dF());
    DecomposedTransform decomp = transform.Decompose();
    EXPECT_DECOMPOSED_TRANSFORM_EQ(
        (DecomposedTransform{
            {0, 0, 0}, {0, 0, 1}, {0, 0, 0}, {0, 0, 0, 1}, {0, 0, 0, 1}}),
        decomp);
    EXPECT_EQ(Transform(transform), Transform::Compose(decomp));
  }
}

}  // namespace
}  // namespace gfx
