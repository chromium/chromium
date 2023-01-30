// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_ripple.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/flood_fill_ink_drop_ripple_test_api.h"

namespace views::test {

TEST(FloodFillInkDropRippleTest, TransformedCenterPointForIrregularClipBounds) {
  const gfx::Size host_size(48, 50);
  const auto clip_insets = gfx::Insets::VH(9, 8);
  const gfx::Point requested_center_point(25, 24);

  // |expected_center_point| is in the coordinate space of ripple's clip bounds
  // defined by |clip_insets|.
  const gfx::Point expected_center_point(
      requested_center_point.x() - clip_insets.left(),
      requested_center_point.y() - clip_insets.top());

  FloodFillInkDropRipple ripple(nullptr, host_size, clip_insets,
                                requested_center_point, SK_ColorWHITE, 0.175f);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  gfx::Point3F actual_center = test_api.MapPoint(
      10, gfx::Point3F(gfx::PointF(test_api.GetDrawnCenterPoint())));

  EXPECT_EQ(expected_center_point,
            gfx::ToRoundedPoint(actual_center.AsPointF()));
}

TEST(FloodFillInkDropRippleTest, MaxDistanceToCorners) {
  const float kAbsError = 0.01f;
  const gfx::Size host_size(70, 130);
  // Rect with the following corners in clockwise order starting at the origin:
  // (10, 30), (60, 30), (10, 100), (60, 100)
  const auto clip_insets = gfx::Insets::VH(30, 10);

  FloodFillInkDropRipple ripple(nullptr, host_size, clip_insets, gfx::Point(),
                                SK_ColorWHITE, 0.175f);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  // Interior points
  EXPECT_NEAR(78.10f, test_api.MaxDistanceToCorners(gfx::Point(10, 40)),
              kAbsError);
  EXPECT_NEAR(71.06f, test_api.MaxDistanceToCorners(gfx::Point(55, 45)),
              kAbsError);
  EXPECT_NEAR(64.03f, test_api.MaxDistanceToCorners(gfx::Point(50, 80)),
              kAbsError);
  EXPECT_NEAR(68.01f, test_api.MaxDistanceToCorners(gfx::Point(20, 85)),
              kAbsError);

  // Exterior points
  EXPECT_NEAR(110.79f, test_api.MaxDistanceToCorners(gfx::Point(3, 5)),
              kAbsError);
  EXPECT_NEAR(108.17f, test_api.MaxDistanceToCorners(gfx::Point(70, 10)),
              kAbsError);
  EXPECT_NEAR(103.08f, test_api.MaxDistanceToCorners(gfx::Point(75, 110)),
              kAbsError);
  EXPECT_NEAR(101.24f, test_api.MaxDistanceToCorners(gfx::Point(5, 115)),
              kAbsError);
}

// Verifies that both going directly from HIDDEN to ACTIVATED state and going
// through PENDING to ACTIVAED state lead to the same final opacity and
// transform values.
TEST(FloodFillInkDropRippleTest, ActivatedFinalState) {
  const float kAbsError = 0.01f;

  const gfx::Size host_size(100, 50);
  const gfx::Point center_point(host_size.width() / 2, host_size.height() / 2);
  const SkColor color = SK_ColorWHITE;
  const float visible_opacity = 0.7f;

  FloodFillInkDropRipple ripple(nullptr, host_size, center_point, color,
                                visible_opacity);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  // Go to ACTIVATED state directly.
  ripple.AnimateToState(InkDropState::ACTIVATED);
  test_api.CompleteAnimations();
  const float activated_opacity = test_api.GetCurrentOpacity();
  const gfx::Transform activated_transform =
      test_api.GetPaintedLayerTransform();

  // Reset state.
  ripple.AnimateToState(InkDropState::HIDDEN);
  test_api.CompleteAnimations();

  // Go to ACTIVATED state through PENDING state.
  ripple.AnimateToState(InkDropState::ACTION_PENDING);
  ripple.AnimateToState(InkDropState::ACTIVATED);
  test_api.CompleteAnimations();
  const float pending_activated_opacity = test_api.GetCurrentOpacity();
  const gfx::Transform pending_activated_transform =
      test_api.GetPaintedLayerTransform();

  // Compare opacity and transform values.
  EXPECT_NEAR(activated_opacity, pending_activated_opacity, kAbsError);
  EXPECT_TRUE(
      activated_transform.ApproximatelyEqual(pending_activated_transform));
}

TEST(FloodFillInkDropRippleTest, TransformIsPixelAligned) {
  const float kEpsilon = 0.001f;

  const gfx::Size host_size(11, 11);
  // Keep the draw center different from the the host center to have a non zero
  // offset in the transformation.
  const gfx::Point center_point(host_size.width() / 3, host_size.height() / 3);
  const SkColor color = SK_ColorYELLOW;
  const float visible_opacity = 0.3f;

  FloodFillInkDropRipple ripple(nullptr, host_size, center_point, color,
                                visible_opacity);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  for (auto dsf : {1.25, 1.33, 1.5, 1.6, 1.75, 1.8, 2.25}) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "Device Scale Factor: " << dsf << std::endl);
    ripple.GetRootLayer()->OnDeviceScaleFactorChanged(dsf);
    gfx::Point3F ripple_origin =
        test_api.MapPoint(host_size.width() / 2, gfx::Point3F());

    // Apply device scale factor to get the final offset.
    gfx::Transform dsf_transform;
    dsf_transform.Scale(dsf, dsf);
    ripple_origin = dsf_transform.MapPoint(ripple_origin);

    EXPECT_NEAR(ripple_origin.x(), std::round(ripple_origin.x()), kEpsilon);
    EXPECT_NEAR(ripple_origin.y(), std::round(ripple_origin.y()), kEpsilon);
  }
}

}  // namespace views::test
