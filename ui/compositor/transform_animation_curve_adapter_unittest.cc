// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_animation_curve_adapter.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

TEST(TransformAnimationCurveAdapterTest, MaximumAnimationScale) {
  auto duration = base::TimeDelta::FromSeconds(1);
  float kArbitraryScale = 123.f;
  float scale = kArbitraryScale;
  EXPECT_TRUE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR,
                                             gfx::Transform(), gfx::Transform(),
                                             duration)
                  .MaximumScale(&scale));
  EXPECT_EQ(1.0f, scale);

  gfx::Transform initial;
  gfx::Transform target;
  initial.Scale(1.0f, 2.0f);
  target.Scale(3.0f, 4.0f);
  scale = kArbitraryScale;
  EXPECT_TRUE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, initial,
                                             target, duration)
                  .MaximumScale(&scale));
  EXPECT_EQ(4.0f, scale);

  scale = kArbitraryScale;
  EXPECT_TRUE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, target,
                                             initial, duration)
                  .MaximumScale(&scale));
  EXPECT_EQ(4.0f, scale);

  target.ApplyPerspectiveDepth(2.0f);
  scale = kArbitraryScale;
  EXPECT_TRUE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, initial,
                                             target, duration)
                  .MaximumScale(&scale));
  EXPECT_EQ(2.0f, scale);
  scale = kArbitraryScale;
  EXPECT_TRUE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, target,
                                             initial, duration)
                  .MaximumScale(&scale));
  EXPECT_EQ(2.0f, scale);

  initial.ApplyPerspectiveDepth(3.0f);
  EXPECT_FALSE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, initial,
                                              target, duration)
                   .MaximumScale(&scale));
  EXPECT_FALSE(TransformAnimationCurveAdapter(gfx::Tween::LINEAR, target,
                                              initial, duration)
                   .MaximumScale(&scale));
}

}  // namespace

}  // namespace ui
