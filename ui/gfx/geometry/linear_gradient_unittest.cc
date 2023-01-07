// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/linear_gradient.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

TEST(LinearGradientTest, Basic) {
  LinearGradient gradient(45);
  EXPECT_TRUE(gradient.IsEmpty());

  gradient.AddStep(.1, 0);
  gradient.AddStep(.5, 50);
  gradient.AddStep(.8, 1);

  EXPECT_FALSE(gradient.IsEmpty());
  EXPECT_EQ(45, gradient.angle());
  EXPECT_EQ(3u, gradient.step_count());
  EXPECT_FLOAT_EQ(gradient.steps()[0].fraction, .1);
  EXPECT_EQ(gradient.steps()[0].alpha, 0);
  EXPECT_FLOAT_EQ(gradient.steps()[1].fraction, .5);
  EXPECT_EQ(gradient.steps()[1].alpha, 50);
  EXPECT_FLOAT_EQ(gradient.steps()[2].fraction, .8);
  EXPECT_EQ(gradient.steps()[2].alpha, 1);

  LinearGradient gradient2(90);
  gradient2.AddStep(.1, 0);
  gradient2.AddStep(.5, 50);
  gradient2.AddStep(.8, 1);

  EXPECT_NE(gradient, gradient2);

  gradient2.set_angle(45);
  EXPECT_EQ(gradient, gradient2);

  gradient2.AddStep(.9, 0);
  EXPECT_NE(gradient, gradient2);
}

TEST(LinearGradientTest, Reverse) {
  LinearGradient gradient(45);
  // Make sure reversing an empty LinearGradient doesn't cause an issue.
  gradient.ReverseSteps();

  gradient.AddStep(.1, 0);
  gradient.AddStep(.5, 50);
  gradient.AddStep(.8, 1);

  gradient.ReverseSteps();

  EXPECT_EQ(45, gradient.angle());
  EXPECT_EQ(3u, gradient.step_count());

  EXPECT_FLOAT_EQ(gradient.steps()[0].fraction, .2);
  EXPECT_EQ(gradient.steps()[0].alpha, 1);
  EXPECT_FLOAT_EQ(gradient.steps()[1].fraction, .5);
  EXPECT_EQ(gradient.steps()[1].alpha, 50);
  EXPECT_FLOAT_EQ(gradient.steps()[2].fraction, .9);
  EXPECT_EQ(gradient.steps()[2].alpha, 0);
}

TEST(LinearGradientTest, ApplyTransform) {
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Translate(10, 50);
    gradient.ApplyTransform(transform);
    EXPECT_EQ(45, gradient.angle());
  }
  // Scale can change the angle.
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Scale(1, 10);
    gradient.ApplyTransform(transform);
    EXPECT_EQ(84, gradient.angle());
  }
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Rotate(45);
    gradient.ApplyTransform(transform);
    EXPECT_EQ(0, gradient.angle());
  }
}

TEST(LinearGradientTest, ApplyAxisTransform2d) {
  {
    LinearGradient gradient(45);
    auto transform = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(1, 1), Vector2dF(10, 50));
    gradient.ApplyTransform(transform);
    EXPECT_EQ(45, gradient.angle());
  }
  // Scale can change the angle.
  {
    LinearGradient gradient(45);
    auto transform = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(1, 10), Vector2dF(10, 50));
    gradient.ApplyTransform(transform);
    EXPECT_EQ(84, gradient.angle());
  }
}

}  // namespace gfx
