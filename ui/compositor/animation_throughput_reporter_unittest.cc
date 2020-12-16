// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/geometry/rect.h"

namespace ui {

using AnimationThroughputReporterTest = AnimationThroughputReporterTestBase;

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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }
  // The animation starts in next frame (16ms) and ends 48 ms later.
  Advance(base::TimeDelta::FromMilliseconds(64));
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }

  // Attach to root after animation setup.
  root_layer()->Add(&layer);
  Advance(base::TimeDelta::FromMilliseconds(64));
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
            1.0f, base::TimeDelta::FromMilliseconds(48))));
  }
  Advance(base::TimeDelta::FromMilliseconds(64));
  run_loop.Run();
}

// Tests animation throughput collection for a persisted animator of a Layer.
TEST_F(AnimationThroughputReporterTest, PersistedAnimation) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(48));
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
  Advance(base::TimeDelta::FromMilliseconds(64));
  run_loop->Run();

  // Report data for animation of opacity goes to 0.5.
  run_loop = std::make_unique<base::RunLoop>();
  layer->SetOpacity(0.5f);
  Advance(base::TimeDelta::FromMilliseconds(64));
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Delete |layer| to abort on-going animations.
  layer.reset();

  // Wait a bit to ensure that report does not happen.
  Advance(base::TimeDelta::FromMilliseconds(100));
}

// Tests no report and no leak when underlying layer is gone before reporter.
TEST_F(AnimationThroughputReporterTest, LayerDestroyedBeforeReporter) {
  auto layer = std::make_unique<Layer>();
  layer->SetOpacity(0.5f);
  root_layer()->Add(layer.get());

  LayerAnimator* animator = layer->GetAnimator();
  AnimationThroughputReporter reporter(
      animator, base::BindLambdaForTesting(
                    [&](const cc::FrameSequenceMetrics::CustomReportData&) {
                      ADD_FAILURE() << "No report for aborted animations.";
                    }));

  {
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Delete |layer| to before the reporter.
  layer.reset();

  // Wait a bit to ensure that report does not happen.
  Advance(base::TimeDelta::FromMilliseconds(100));
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer->SetOpacity(1.0f);
  }

  // Detach from the root and attach to a root.
  root_layer()->Remove(layer.get());
  root_layer()->Add(layer.get());

  // Wait a bit to ensure that report does not happen.
  Advance(base::TimeDelta::FromMilliseconds(100));
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
  Advance(base::TimeDelta::FromMilliseconds(100));

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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer->SetOpacity(1.0f);
    layer->SetBounds(gfx::Rect(0, 0, 5, 6));
  }
  Advance(base::TimeDelta::FromMilliseconds(64));
  run_loop.Run();
}

}  // namespace ui
