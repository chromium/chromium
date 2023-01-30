// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/mask_filter_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {
namespace {

LinearGradient CreateGradient(int angle) {
  LinearGradient gradient(angle);
  gradient.AddStep(0.5, 50);
  return gradient;
}

TEST(MaskFilterInfoTest, ApplyTransform) {
  MaskFilterInfo info(RRectF(1.f, 2.f, 20.f, 25.f, 5.f));
  MaskFilterInfo expected = info;
  info.ApplyTransform(Transform());
  EXPECT_EQ(expected, info);

  auto translation = Transform::MakeTranslation(-3.5f, 7.75f);
  expected = MaskFilterInfo(RRectF(-2.5f, 9.75f, 20.f, 25.f, 5.f));
  info.ApplyTransform(translation);
  EXPECT_EQ(expected, info);

  info = MaskFilterInfo(RRectF(1.f, 2.f, 20.f, 25.f, 5.f), CreateGradient(50));
  expected =
      MaskFilterInfo(RRectF(-2.5f, 9.75f, 20.f, 25.f, 5.f), CreateGradient(50));
  info.ApplyTransform(translation);
  EXPECT_EQ(expected, info);

  auto rotation_90_clock = Transform::Make90degRotation();
  info = MaskFilterInfo(
      RRectF(RectF(0, 0, 20.f, 25.f), 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f),
      CreateGradient(50));
  expected = MaskFilterInfo(RRectF(RectF(-25.f, 0, 25.f, 20.f), 8.f, 7.f, 2.f,
                                   1.f, 4.f, 3.f, 6.f, 5.f),
                            CreateGradient(-40));
  info.ApplyTransform(rotation_90_clock);
  EXPECT_EQ(expected, info);

  Transform rotation_90_unrounded;
  rotation_90_unrounded.Rotate(90.0 + 1e-10);
  info = MaskFilterInfo(
      RRectF(RectF(0, 0, 20.f, 25.f), 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f),
      CreateGradient(50));
  EXPECT_TRUE(rotation_90_unrounded.Preserves2dAxisAlignment());
  info.ApplyTransform(rotation_90_unrounded);
  EXPECT_EQ(expected, info);

  auto scale = Transform::MakeScale(2.f, 3.f);
  info = MaskFilterInfo(
      RRectF(RectF(0, 0, 20.f, 25.f), 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f),
      CreateGradient(50));
  expected = MaskFilterInfo(RRectF(RectF(0, 0, 40.f, 75.f), 2.f, 6.f, 6.f, 12.f,
                                   10.f, 18.f, 14.f, 24.f),
                            CreateGradient(61));
  info.ApplyTransform(scale);
  EXPECT_EQ(expected, info);

  Transform rotation;
  rotation.Rotate(45);
  info.ApplyTransform(rotation);
  EXPECT_TRUE(info.IsEmpty());
}

TEST(MaskFilterInfoTest, ApplyAxisTransform2d) {
  MaskFilterInfo info(
      RRectF(RectF(0, 0, 20.f, 25.f), 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f),
      CreateGradient(50));
  MaskFilterInfo expected = info;
  info.ApplyTransform(AxisTransform2d());
  EXPECT_EQ(expected, info);

  MaskFilterInfo scaled = info;
  expected = MaskFilterInfo(RRectF(RectF(0, 0, 40.f, 75.f), 2.f, 6.f, 6.f, 12.f,
                                   10.f, 18.f, 14.f, 24.f),
                            CreateGradient(61));
  scaled.ApplyTransform(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(2.f, 3.f), Vector2dF()));
  EXPECT_EQ(expected, scaled);

  MaskFilterInfo scaled_translated = scaled;
  expected = MaskFilterInfo(RRectF(RectF(-3.5f, 7.75f, 40.f, 75.f), 2.f, 6.f,
                                   6.f, 12.f, 10.f, 18.f, 14.f, 24.f),
                            CreateGradient(61));
  scaled_translated.ApplyTransform(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(1.f, 1.f), Vector2dF(-3.5f, 7.75f)));
  EXPECT_EQ(expected, scaled_translated);

  MaskFilterInfo scaled_translated_2 = info;
  scaled_translated_2.ApplyTransform(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(2.f, 3.f), Vector2dF(-3.5f, 7.75f)));
  EXPECT_EQ(expected, scaled_translated_2);

  const float kInf = std::numeric_limits<float>::infinity();
  const float kNan = std::numeric_limits<float>::quiet_NaN();
  auto failure_is_empty = [&](const AxisTransform2d& transform) {
    MaskFilterInfo transformed = info;
    transformed.ApplyTransform(transform);
    return transformed.IsEmpty();
  };
  EXPECT_TRUE(failure_is_empty(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(kInf, 1), Vector2dF(1, 1))));
  EXPECT_TRUE(failure_is_empty(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(kNan, 1), Vector2dF(1, 1))));
  EXPECT_TRUE(failure_is_empty(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(1, 1), Vector2dF(1, kInf))));
  EXPECT_TRUE(failure_is_empty(AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(1, 1), Vector2dF(1, kNan))));
}

}  // anonymous namespace
}  // namespace gfx
