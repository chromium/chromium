// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_throughput_reporter.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class AnimationThroughputReporterTest : public testing::Test {
 public:
  AnimationThroughputReporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  AnimationThroughputReporterTest(const AnimationThroughputReporterTest&) =
      delete;
  AnimationThroughputReporterTest& operator=(
      const AnimationThroughputReporterTest&) = delete;
  ~AnimationThroughputReporterTest() override = default;

  // testing::Test:
  void SetUp() override {
    context_factories_ = std::make_unique<TestContextFactories>(false);

    const gfx::Rect bounds(100, 100);
    host_.reset(TestCompositorHost::Create(
        bounds, context_factories_->GetContextFactory()));
    host_->Show();

    compositor()->SetRootLayer(&root_);

    frame_generation_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(50), this,
        &AnimationThroughputReporterTest::GenerateOneFrame);
  }

  void TearDown() override {
    frame_generation_timer_.Stop();
    host_.reset();
    context_factories_.reset();
  }

  void GenerateOneFrame() { compositor()->ScheduleFullRedraw(); }

  Compositor* compositor() { return host_->GetCompositor(); }
  Layer* root_layer() { return &root_; }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestContextFactories> context_factories_;
  std::unique_ptr<TestCompositorHost> host_;
  Layer root_;

  // A timer to generate continuous compositor frames to trigger throughput
  // data being transferred back.
  base::RepeatingTimer frame_generation_timer_;
};

// Tests animation throughput collection with implicit animation scenario.
TEST_F(AnimationThroughputReporterTest, ImplicitAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  base::RunLoop run_loop;
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        run_loop.Quit();
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer.SetOpacity(1.0f);
  }
  run_loop.Run();
}

// Tests animation throughput collection with implicit animation setup before
// Layer is attached to a compositor.
TEST_F(AnimationThroughputReporterTest, ImplicitAnimationLateAttach) {
  Layer layer;
  layer.SetOpacity(0.5f);

  base::RunLoop run_loop;
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        run_loop.Quit();
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer.SetOpacity(1.0f);
  }

  // Attach to root after animation setup.
  root_layer()->Add(&layer);

  run_loop.Run();
}

// Tests animation throughput collection with explicitly created animation
// sequence scenario.
TEST_F(AnimationThroughputReporterTest, ExplicitAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  base::RunLoop run_loop;
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        run_loop.Quit();
                      }));

    animator->ScheduleAnimation(
        new LayerAnimationSequence(LayerAnimationElement::CreateOpacityElement(
            1.0f, base::TimeDelta::FromMilliseconds(50))));
  }
  run_loop.Run();
}

// Tests animation throughput collection for a persisted animator of a Layer.
TEST_F(AnimationThroughputReporterTest, PersistedAnimation) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(50));
  layer->SetAnimator(animator);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  // |reporter| keeps reporting as long as it is alive.
  AnimationThroughputReporter reporter(
      animator, base::BindLambdaForTesting(
                    [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                      run_loop->Quit();
                    }));

  // Report data for animation of opacity goes to 1.
  layer->SetOpacity(1.0f);
  run_loop->Run();

  // Report data for animation of opacity goes to 0.5.
  run_loop = std::make_unique<base::RunLoop>();
  layer->SetOpacity(0.5f);
  run_loop->Run();
}

// Tests animation throughput not reported when animation is aborted.
TEST_F(AnimationThroughputReporterTest, AbortedAnimation) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  {
    LayerAnimator* animator = layer->GetAnimator();
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        ADD_FAILURE() << "No report for aborted animations.";
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer->SetOpacity(1.0f);
  }

  // Delete |layer| to abort on-going animations.
  layer.reset();

  // Wait a bit to ensure that report does not happen.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();
}

// Tests animation throughput not reported when detached from timeline.
TEST_F(AnimationThroughputReporterTest, NoReportOnDetach) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  {
    LayerAnimator* animator = layer->GetAnimator();
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        ADD_FAILURE() << "No report for aborted animations.";
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer->SetOpacity(1.0f);
  }

  // Detach from the root and attach to a root.
  root_layer()->Remove(layer.get());
  root_layer()->Add(layer.get());

  // Wait a bit to ensure that report does not happen.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();
}

// Tests animation throughput not reported and no leak when animation is stopped
// without being attached to a root.
TEST_F(AnimationThroughputReporterTest, EndDetachedNoReportNoLeak) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);

  LayerAnimator* animator = layer->GetAnimator();

  // Schedule an animation without being attached to a root.
  {
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        ADD_FAILURE() << "No report for aborted animations.";
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer->SetOpacity(1.0f);
  }

  // End the animation without being attached to a root.
  animator->StopAnimating();

  // Wait a bit to ensure that report does not happen.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  // AnimationTracker in |reporter| should not leak in asan.
}

// Tests animation throughput are reported if there was a previous animation
// preempted under IMMEDIATELY_ANIMATE_TO_NEW_TARGET strategy.
TEST_F(AnimationThroughputReporterTest, ReportForAnimateToNewTarget) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.f);
  layer->SetBounds(gfx::Rect(0, 0, 1, 2));
  root_layer()->Add(layer.get());

  LayerAnimator* animator = layer->GetAnimator();

  // Schedule an animation that will be preempted. No report should happen.
  {
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        ADD_FAILURE() << "No report for aborted animations.";
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer->SetOpacity(0.5f);
    layer->SetBounds(gfx::Rect(0, 0, 3, 4));
  }

  // Animate to new target. Report should happen.
  base::RunLoop run_loop;
  {
    AnimationThroughputReporter reporter(
        animator, base::BindLambdaForTesting(
                      [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                        run_loop.Quit();
                      }));

    ScopedLayerAnimationSettings settings(animator);
    settings.SetPreemptionStrategy(
        LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
    layer->SetOpacity(1.0f);
    layer->SetBounds(gfx::Rect(0, 0, 5, 6));
  }
  run_loop.Run();
}

}  // namespace ui
