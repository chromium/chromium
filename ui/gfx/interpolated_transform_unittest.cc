// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/test/geometry_util.h"

TEST(InterpolatedTransformTest, InterpolatedRotation) {
  ui::InterpolatedRotation interpolated_rotation(0, 100);
  ui::InterpolatedRotation interpolated_rotation_diff_start_end(
      0, 100, 100, 200);

  for (int i = 0; i <= 100; ++i) {
    gfx::Transform rotation;
    rotation.Rotate(i);
    gfx::Transform interpolated = interpolated_rotation.Interpolate(i / 100.0f);
    EXPECT_TRANSFORM_EQ(rotation, interpolated);
    interpolated = interpolated_rotation_diff_start_end.Interpolate(i + 100);
    EXPECT_TRANSFORM_EQ(rotation, interpolated);
  }
}

TEST(InterpolatedTransformTest, InterpolatedScale) {
  ui::InterpolatedScale interpolated_scale(gfx::Point3F(0, 0, 0),
                                           gfx::Point3F(100, 100, 100));
  ui::InterpolatedScale interpolated_scale_diff_start_end(
      gfx::Point3F(0, 0, 0), gfx::Point3F(100, 100, 100), 100, 200);

  for (int i = 0; i <= 100; ++i) {
    gfx::Transform scale;
    scale.Scale3d(i, i, i);
    gfx::Transform interpolated = interpolated_scale.Interpolate(i / 100.0f);
    EXPECT_TRANSFORM_EQ(scale, interpolated);
    interpolated = interpolated_scale_diff_start_end.Interpolate(i + 100);
    EXPECT_TRANSFORM_EQ(scale, interpolated);
  }
}

TEST(InterpolatedTransformTest, InterpolatedTranslate) {
  ui::InterpolatedTranslation interpolated_xform(gfx::PointF(),
                                                 gfx::PointF(100.f, 100.f));

  ui::InterpolatedTranslation interpolated_xform_diff_start_end(
      gfx::PointF(), gfx::PointF(100.f, 100.f), 100, 200);

  for (int i = 0; i <= 100; ++i) {
    gfx::Transform xform;
    xform.Translate(i, i);
    gfx::Transform interpolated = interpolated_xform.Interpolate(i / 100.0f);
    EXPECT_TRANSFORM_EQ(xform, interpolated);
    interpolated = interpolated_xform_diff_start_end.Interpolate(i + 100);
    EXPECT_TRANSFORM_EQ(xform, interpolated);
  }
}

TEST(InterpolatedTransformTest, InterpolatedTranslate3d) {
  ui::InterpolatedTranslation interpolated_xform(gfx::Point3F(0, 0, 0),
                                                 gfx::Point3F(100, 100, 100));

  ui::InterpolatedTranslation interpolated_xform_diff_start_end(
      gfx::Point3F(0, 0, 0), gfx::Point3F(100, 100, 100), 100, 200);

  for (int i = 0; i <= 100; ++i) {
    gfx::Transform xform;
    xform.Translate3d(i, i, i);
    gfx::Transform interpolated = interpolated_xform.Interpolate(i / 100.0f);
    EXPECT_TRANSFORM_EQ(xform, interpolated);
    interpolated = interpolated_xform_diff_start_end.Interpolate(i + 100);
    EXPECT_TRANSFORM_EQ(xform, interpolated);
  }
}

TEST(InterpolatedTransformTest, InterpolatedRotationAboutPivot) {
  gfx::Point pivot(100, 100);
  gfx::Point above_pivot(100, 200);
  ui::InterpolatedRotation rot(0, 90);
  ui::InterpolatedTransformAboutPivot interpolated_xform(
      pivot, std::make_unique<ui::InterpolatedRotation>(0, 90));
  gfx::Transform result = interpolated_xform.Interpolate(0.0f);
  EXPECT_TRANSFORM_EQ(gfx::Transform(), result);
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  pivot = result.MapPoint(pivot);
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(0, 100);
  above_pivot = result.MapPoint(above_pivot);
  EXPECT_EQ(expected_result, above_pivot);
}

TEST(InterpolatedTransformTest, InterpolatedScaleAboutPivot) {
  gfx::Point pivot(100, 100);
  gfx::Point above_pivot(100, 200);
  ui::InterpolatedTransformAboutPivot interpolated_xform(
      pivot, std::make_unique<ui::InterpolatedScale>(gfx::Point3F(1, 1, 1),
                                                     gfx::Point3F(2, 2, 2)));
  gfx::Transform result = interpolated_xform.Interpolate(0.0f);
  EXPECT_TRANSFORM_EQ(gfx::Transform(), result);
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  pivot = result.MapPoint(pivot);
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(100, 300);
  above_pivot = result.MapPoint(above_pivot);
  EXPECT_EQ(expected_result, above_pivot);
}

ui::InterpolatedTransform* GetScreenRotation(int degrees, bool reversed) {
  gfx::Point old_pivot;
  gfx::Point new_pivot;

  int width = 1920;
  int height = 180;

  switch (degrees) {
    case 90:
      new_pivot = gfx::Point(width, 0);
      break;
    case -90:
      new_pivot = gfx::Point(0, height);
      break;
    case 180:
    case 360:
      new_pivot = old_pivot = gfx::Point(width / 2, height / 2);
      break;
  }

  std::unique_ptr<ui::InterpolatedTransform> rotation =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          old_pivot, std::make_unique<ui::InterpolatedRotation>(
                         reversed ? degrees : 0, reversed ? 0 : degrees));

  std::unique_ptr<ui::InterpolatedTransform> translation =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(), gfx::PointF(new_pivot.x() - old_pivot.x(),
                                     new_pivot.y() - old_pivot.y()));

  float scale_factor = 0.9f;
  std::unique_ptr<ui::InterpolatedTransform> scale_down =
      std::make_unique<ui::InterpolatedScale>(1.0f, scale_factor, 0.0f, 0.5f);

  std::unique_ptr<ui::InterpolatedTransform> scale_up =
      std::make_unique<ui::InterpolatedScale>(1.0f, 1.0f / scale_factor, 0.5f,
                                              1.0f);

  std::unique_ptr<ui::InterpolatedTransform> to_return =
      std::make_unique<ui::InterpolatedConstantTransform>(gfx::Transform());

  scale_up->SetChild(std::move(scale_down));
  translation->SetChild(std::move(scale_up));
  rotation->SetChild(std::move(translation));
  to_return->SetChild(std::move(rotation));
  to_return->SetReversed(reversed);

  return to_return.release();
}

TEST(InterpolatedTransformTest, ScreenRotationEndsCleanly) {
  for (int i = 0; i < 2; ++i) {
    for (int degrees = -360; degrees <= 360; degrees += 90) {
      const bool reversed = i == 1;
      std::unique_ptr<ui::InterpolatedTransform> screen_rotation(
          GetScreenRotation(degrees, reversed));
      gfx::Transform interpolated = screen_rotation->Interpolate(1.0f);
      // Upper-left 3x3 matrix should all be 0, 1 or -1.
      for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
          float entry = interpolated.rc(row, col);
          EXPECT_TRUE(entry == 0 || entry == 1 || entry == -1);
        }
      }
    }
  }
}

ui::InterpolatedTransform* GetMaximize() {
  gfx::Rect target_bounds(0, 0, 1920, 1080);
  gfx::Rect initial_bounds(30, 1000, 192, 108);

  float scale_x = static_cast<float>(
      target_bounds.height()) / initial_bounds.width();
  float scale_y = static_cast<float>(
      target_bounds.width()) / initial_bounds.height();

  std::unique_ptr<ui::InterpolatedTransform> scale =
      std::make_unique<ui::InterpolatedScale>(
          gfx::Point3F(1, 1, 1), gfx::Point3F(scale_x, scale_y, 1));

  std::unique_ptr<ui::InterpolatedTransform> translation =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(), gfx::PointF(target_bounds.x() - initial_bounds.x(),
                                     target_bounds.y() - initial_bounds.y()));

  std::unique_ptr<ui::InterpolatedTransform> rotation =
      std::make_unique<ui::InterpolatedRotation>(0, 4.0f);

  std::unique_ptr<ui::InterpolatedTransform> rotation_about_pivot(
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          gfx::Point(initial_bounds.width() * 0.5,
                     initial_bounds.height() * 0.5),
          std::move(rotation)));

  scale->SetChild(std::move(translation));
  rotation_about_pivot->SetChild(std::move(scale));

  rotation_about_pivot->SetReversed(true);

  return rotation_about_pivot.release();
}

TEST(InterpolatedTransformTest, MaximizeEndsCleanly) {
  std::unique_ptr<ui::InterpolatedTransform> maximize(GetMaximize());
  gfx::Transform interpolated = maximize->Interpolate(1.0f);
  // Upper-left 3x3 matrix should all be 0, 1 or -1.
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      float entry = interpolated.rc(row, col);
      EXPECT_TRUE(entry == 0 || entry == 1 || entry == -1);
    }
  }
}
