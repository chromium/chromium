// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/linear_gradient.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

TEST(LinearGradientTest, Basic) {
  LinearGradient gradient(45);
  EXPECT_TRUE(gradient.IsEmpty());

  gradient.AddStep(10, 0);
  gradient.AddStep(50, 50);
  gradient.AddStep(80, 1);

  EXPECT_FALSE(gradient.IsEmpty());
  EXPECT_EQ(45, gradient.angle());
  EXPECT_EQ(3u, gradient.step_count());
  EXPECT_EQ(gradient.steps()[0].percent, 10);
  EXPECT_EQ(gradient.steps()[0].alpha, 0);
  EXPECT_EQ(gradient.steps()[1].percent, 50);
  EXPECT_EQ(gradient.steps()[1].alpha, 50);
  EXPECT_EQ(gradient.steps()[2].percent, 80);
  EXPECT_EQ(gradient.steps()[2].alpha, 1);

  LinearGradient gradient2(90);
  gradient2.AddStep(10, 0);
  gradient2.AddStep(50, 50);
  gradient2.AddStep(80, 1);

  EXPECT_NE(gradient, gradient2);

  gradient2.set_angle(45);
  EXPECT_EQ(gradient, gradient2);

  gradient2.AddStep(90, 0);
  EXPECT_NE(gradient, gradient2);
}

TEST(LinearGradientTest, Reverse) {
  LinearGradient gradient(45);
  // Make sure reversing an empty LinearGradient doesn't cause an issue.
  gradient.ReverseSteps();

  gradient.AddStep(10, 0);
  gradient.AddStep(50, 50);
  gradient.AddStep(80, 1);

  gradient.ReverseSteps();

  EXPECT_EQ(45, gradient.angle());
  EXPECT_EQ(3u, gradient.step_count());

  EXPECT_EQ(gradient.steps()[0].percent, 20);
  EXPECT_EQ(gradient.steps()[0].alpha, 1);
  EXPECT_EQ(gradient.steps()[1].percent, 50);
  EXPECT_EQ(gradient.steps()[1].alpha, 50);
  EXPECT_EQ(gradient.steps()[2].percent, 90);
  EXPECT_EQ(gradient.steps()[2].alpha, 0);
}

TEST(LinearGradientTest, Rotate) {
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Translate(10, 50);
    gradient.Transform(transform);
    EXPECT_EQ(45, gradient.angle());
  }
  // Scale can change the angle.
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Scale(1, 10);
    gradient.Transform(transform);
    EXPECT_EQ(84, gradient.angle());
  }
  {
    LinearGradient gradient(45);
    gfx::Transform transform;
    transform.Rotate(45);
    gradient.Transform(transform);
    EXPECT_EQ(0, gradient.angle());
  }
}

}  // namespace gfx
