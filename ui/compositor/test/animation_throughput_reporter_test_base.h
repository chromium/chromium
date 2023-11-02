// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_ANIMATION_THROUGHPUT_REPORTER_TEST_BASE_H_
#define UI_COMPOSITOR_TEST_ANIMATION_THROUGHPUT_REPORTER_TEST_BASE_H_

#include <memory>

#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_compositor_host.h"

namespace base {
class RunLoop;
}

namespace ui {
class TestContextFactories;

class AnimationThroughputReporterTestBase : public testing::Test {
 public:
  AnimationThroughputReporterTestBase();
  AnimationThroughputReporterTestBase(
      const AnimationThroughputReporterTestBase&) = delete;
  AnimationThroughputReporterTestBase& operator=(
      const AnimationThroughputReporterTestBase&) = delete;
  ~AnimationThroughputReporterTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void GenerateOneFrame() { compositor()->ScheduleFullRedraw(); }

  Compositor* compositor() { return host_->GetCompositor(); }
  Layer* root_layer() { return &root_; }

  // Advances the time by |delta|.
  void Advance(const base::TimeDelta& delta);

  void QuitRunLoop();

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<TestContextFactories> context_factories_;
  std::unique_ptr<TestCompositorHost> host_;
  Layer root_;

  // A timer to generate continuous compositor frames to trigger throughput
  // data being transferred back.
  base::RepeatingTimer frame_generation_timer_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_ANIMATION_THROUGHPUT_REPORTER_TEST_BASE_H_
