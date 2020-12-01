// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/animation_throughput_reporter_test_base.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

AnimationThroughputReporterTestBase::AnimationThroughputReporterTestBase() =
    default;
AnimationThroughputReporterTestBase::~AnimationThroughputReporterTestBase() =
    default;

void AnimationThroughputReporterTestBase::SetUp() {
  context_factories_ = std::make_unique<TestContextFactories>(false);

  const gfx::Rect bounds(300, 300);
  host_.reset(TestCompositorHost::Create(
      bounds, context_factories_->GetContextFactory()));
  host_->Show();

  compositor()->SetRootLayer(&root_);

  frame_generation_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(16), this,
      &AnimationThroughputReporterTestBase::GenerateOneFrame);
}

void AnimationThroughputReporterTestBase::TearDown() {
  frame_generation_timer_.Stop();
  host_.reset();
  context_factories_.reset();
}

void AnimationThroughputReporterTestBase::Advance(
    const base::TimeDelta& delta) {
  task_environment_.FastForwardBy(delta);
}

}  // namespace ui
