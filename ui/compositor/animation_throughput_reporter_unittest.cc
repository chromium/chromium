// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_throughput_reporter.h"

#include <memory>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/animation_throughput_reporter_test_base.h"
#include "ui/compositor/test/throughput_report_checker.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

using AnimationThroughputReporterTest = AnimationThroughputReporterTestBase;

// Tests animation throughput collection with implicit animation scenario.
TEST_F(AnimationThroughputReporterTest, ImplicitAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this);
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer.SetOpacity(1.0f);
  }
  // The animation starts in next frame (16ms) and ends 48 ms later.
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests animation throughput collection with implicit animation setup before
// Layer is attached to a compositor.
TEST_F(AnimationThroughputReporterTest, ImplicitAnimationLateAttach) {
  Layer layer;
  layer.SetOpacity(0.5f);

  ThroughputReportChecker checker(this);
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer.SetOpacity(1.0f);
  }

  // Attach to root after animation setup.
  root_layer()->Add(&layer);
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests animation throughput collection with explicitly created animation
// sequence scenario.
TEST_F(AnimationThroughputReporterTest, ExplicitAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this);
  LayerAnimator* animator = layer.GetAnimator();
  AnimationThroughputReporter reporter(animator, checker.repeating_callback());

  animator->ScheduleAnimation(
      new LayerAnimationSequence(LayerAnimationElement::CreateOpacityElement(
          1.0f, base::Milliseconds(48))));

  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests animation throughput collection for a persisted animator of a Layer.
TEST_F(AnimationThroughputReporterTest, PersistedAnimation) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  // Set a persisted animator to |layer|.
  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(48));
  layer->SetAnimator(animator);

  // |reporter| keeps reporting as long as it is alive.
  ThroughputReportChecker checker(this);
  AnimationThroughputReporter reporter(animator, checker.repeating_callback());

  // Report data for animation of opacity goes to 1.
  layer->SetOpacity(1.0f);
  EXPECT_TRUE(checker.WaitUntilReported());

  // Report data for animation of opacity goes to 0.5.
  checker.reset();
  layer->SetOpacity(0.5f);
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests animation throughput not reported when animation is aborted.
TEST_F(AnimationThroughputReporterTest, AbortedAnimation) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);

  // Reporter started monitoring animation, then deleted, which should be
  // reported when the animation ends.
  {
    LayerAnimator* animator = layer->GetAnimator();
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Delete |layer| to abort on-going animations.
  layer.reset();

  // Wait a bit to ensure that report does not happen.
  Advance(base::Milliseconds(100));

  // TODO(crbug.com/40161328): Test the scenario where the report exists when
  // the layer is removed.
}

// Tests no report and no leak when underlying layer is gone before reporter.
TEST_F(AnimationThroughputReporterTest, LayerDestroyedBeforeReporter) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  LayerAnimator* animator = layer->GetAnimator();
  AnimationThroughputReporter reporter(animator, checker.repeating_callback());
  {
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Delete |layer| to before the reporter.
  layer.reset();

  // Wait a bit to ensure that report does not happen.
  Advance(base::Milliseconds(100));
}

// Tests animation throughput not reported when detached from timeline.
TEST_F(AnimationThroughputReporterTest, NoReportOnDetach) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  {
    LayerAnimator* animator = layer->GetAnimator();
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Detach from the root and attach to a root.
  root_layer()->Remove(layer.get());
  root_layer()->Add(layer.get());

  // Wait a bit to ensure that report does not happen.
  Advance(base::Milliseconds(100));
}

// Tests animation throughput not reported and no leak when animation is stopped
// without being attached to a root.
TEST_F(AnimationThroughputReporterTest, EndDetachedNoReportNoLeak) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  LayerAnimator* animator = layer->GetAnimator();
  // Schedule an animation without being attached to a root.
  {
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(50));
    layer->SetOpacity(1.0f);
  }

  // End the animation without being attached to a root.
  animator->StopAnimating();

  // Wait a bit to ensure that report does not happen.
  Advance(base::Milliseconds(100));

  // AnimationTracker in |reporter| should not leak in asan.
}

// Tests animation throughput are reported if there was a previous animation
// preempted under IMMEDIATELY_ANIMATE_TO_NEW_TARGET strategy.
TEST_F(AnimationThroughputReporterTest, ReportForAnimateToNewTarget) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.f);
  layer->SetBounds(gfx::Rect(0, 0, 1, 2));
  root_layer()->Add(layer.get());

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  LayerAnimator* animator = layer->GetAnimator();
  // Schedule an animation that will be preempted. No report should happen.
  {
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(50));
    layer->SetOpacity(0.5f);
    layer->SetBounds(gfx::Rect(0, 0, 3, 4));
  }

  // Animate to new target. Report should happen.
  ThroughputReportChecker checker2(this);
  {
    AnimationThroughputReporter reporter(animator,
                                         checker2.repeating_callback());
    ScopedLayerAnimationSettings settings(animator);
    settings.SetPreemptionStrategy(
        LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer->SetOpacity(1.0f);
    layer->SetBounds(gfx::Rect(0, 0, 5, 6));
  }
  EXPECT_TRUE(checker2.WaitUntilReported());
}

// Tests AnimationThroughputReporter does not leak its AnimationTracker when
// there are existing animations but no new animation sequence starts after it
// is created.
TEST_F(AnimationThroughputReporterTest, NoLeakWithNoAnimationStart) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  LayerAnimator* animator = layer->GetAnimator();

  // Create an existing animation.
  {
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(50));
    layer->SetOpacity(0.0f);
  }

  // Create the reporter with the existing animation.
  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  {
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());
  }

  // Wait a bit to ensure to let the existing animation finish.
  // There should be no report and no leak.
  Advance(base::Milliseconds(100));
}

// Tests smoothness is not reported if the animation will not run.
TEST_F(AnimationThroughputReporterTest, NoReportForNoRunAnimations) {
  Layer layer;
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this, /*fail_if_reported=*/true);
  {
    LayerAnimator* animator = layer.GetAnimator();
    AnimationThroughputReporter reporter(animator,
                                         checker.repeating_callback());

    // Simulate views::AnimationBuilder to create an animation that will not
    // run.
    ScopedLayerAnimationSettings settings(animator);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    std::vector<ui::LayerAnimationSequence*> sequences;
    sequences.push_back(new LayerAnimationSequence(
        LayerAnimationElement::CreateOpacityElement(1.0f, base::TimeDelta())));
    animator->StartTogether(std::move(sequences));
  }

  // Wait a bit to ensure that report does not happen.
  Advance(base::Milliseconds(100));
}

}  // namespace ui
