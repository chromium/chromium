// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/total_animation_throughput_reporter.h"

#include <memory>

#include "base/run_loop.h"
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

#if defined(OS_WIN)
// TestCompositor doesn't work with MOCK_TIME on Windows. crbug.com/1152103
#define MAYBE_SingleAnimation DISABLED_SingleAnimation
#define MAYBE_StopAnimation DISABLED_StopAnimation
#define MAYBE_MultipleAnimations DISABLED_MultipleAnimations
#define MAYBE_MultipleAnimationsOnSingleLayer \
  DISABLED_MultipleAnimationsOnSingleLayer
#define MAYBE_AddAnimationWhileAnimating DISABLED_AddAnimationWhileAnimating
#define MAYBE_RemoveWhileAnimating DISABLED_RemoveWhileAnimating
#define MAYBE_StartWhileAnimating DISABLED_StartWhileAnimating
#define MAYBE_PersistedAnimation DISABLED_PersistedAnimation
#define MAYBE_OnceReporter DISABLED_OnceReporter
#define MAYBE_OnceReporterShouldDelete DISABLED_OnceReporterShouldDelete
#else
#define MAYBE_SingleAnimation SingleAnimation
#define MAYBE_StopAnimation StopAnimation
#define MAYBE_MultipleAnimations MultipleAnimations
#define MAYBE_MultipleAnimationsOnSingleLayer MultipleAnimationsOnSingleLayer
#define MAYBE_AddAnimationWhileAnimating AddAnimationWhileAnimating
#define MAYBE_RemoveWhileAnimating RemoveWhileAnimating
#define MAYBE_StartWhileAnimating StartWhileAnimating
#define MAYBE_PersistedAnimation PersistedAnimation
#define MAYBE_OnceReporter OnceReporter
#define MAYBE_OnceReporterShouldDelete OnceReporterShouldDelete
#endif

namespace ui {
namespace {

class TestReporter : public TotalAnimationThroughputReporter {
 public:
  explicit TestReporter(ui::Compositor* compositor)
      : ui::TotalAnimationThroughputReporter(
            compositor,
            base::BindRepeating(&TestReporter::Reported,
                                base::Unretained(this))) {}
  TestReporter(ui::Compositor* compositor, bool should_delete)
      : ui::TotalAnimationThroughputReporter(
            compositor,
            base::BindOnce(&TestReporter::Reported, base::Unretained(this)),
            should_delete) {}

  TestReporter(const TestReporter&) = delete;
  TestReporter& operator=(const TestReporter&) = delete;
  ~TestReporter() override = default;

  bool reported() const { return reported_; }

  void reset() { reported_ = false; }

 private:
  void Reported(const cc::FrameSequenceMetrics::CustomReportData&) {
    reported_ = true;
  }

  bool reported_ = false;
};

}  // namespace

using TotalAnimationThroughputReporterTest =
    AnimationThroughputReporterTestBase;

TEST_F(TotalAnimationThroughputReporterTest, MAYBE_SingleAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_TRUE(reporter.reported());
}

// Tests the stopping last animation will trigger the animation.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_StopAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }

  Advance(base::TimeDelta::FromMilliseconds(16));
  EXPECT_FALSE(reporter.reported());
  layer.GetAnimator()->StopAnimating();
  Advance(base::TimeDelta::FromMilliseconds(16));
  EXPECT_TRUE(reporter.reported());
}

// Tests the longest animation will trigger the report.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_MultipleAnimations) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer1.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer1.SetOpacity(1.0f);
  }
  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(96));
    layer2.SetOpacity(1.0f);
  }

  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());
  Advance(base::TimeDelta::FromMilliseconds(200));
  EXPECT_TRUE(reporter.reported());
}

// Tests the longest animation on a single layer will triger the report.
TEST_F(TotalAnimationThroughputReporterTest,
       MAYBE_MultipleAnimationsOnSingleLayer) {
  Layer layer;
  layer.SetOpacity(0.5f);
  layer.SetLayerBrightness(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(96));
    layer.SetLayerBrightness(1.0f);
  }

  Advance(base::TimeDelta::FromMilliseconds(64));
  EXPECT_FALSE(reporter.reported());
  Advance(base::TimeDelta::FromMilliseconds(48));
  EXPECT_TRUE(reporter.reported());
}

// Tests adding new animation will extends the duration.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_AddAnimationWhileAnimating) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer1.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer1.SetOpacity(1.0f);
  }

  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());

  // Add new animation while animating.
  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer2.SetOpacity(1.0f);
  }

  // The animation time is extended.
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());

  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_TRUE(reporter.reported());
}

// Tests removing last animation will call report callback.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_RemoveWhileAnimating) {
  auto layer1 = std::make_unique<Layer>();
  layer1->SetOpacity(0.5f);
  root_layer()->Add(layer1.get());

  TestReporter reporter(compositor());
  {
    LayerAnimator* animator = layer1->GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
    layer1->SetOpacity(1.0f);
  }

  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer2.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(48));
  EXPECT_FALSE(reporter.reported());
  layer1.reset();
  // Aborting will be processed in next frame.
  Advance(base::TimeDelta::FromMilliseconds(16));
  EXPECT_TRUE(reporter.reported());
}

// Make sure the reporter can start measuring even if the animation
// has started.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_StartWhileAnimating) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(16));

  TestReporter reporter(compositor());
  EXPECT_TRUE(reporter.IsMeasuringForTesting());
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());
}

// Tests the reporter is called multiple times for persistent animation.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_PersistedAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(48));
  layer.SetAnimator(animator);

  // |reporter| keeps reporting as long as it is alive.
  TestReporter reporter(compositor());

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());

  // Report data for animation of opacity goes to 0.5.
  reporter.reset();
  layer.SetOpacity(0.5f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());
}

// Make sure the once reporter is called only once.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_OnceReporter) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(32));
  layer.SetAnimator(animator);

  TestReporter reporter(compositor(), /*should_delete=*/false);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());

  // Report data for animation of opacity goes to 0.5.
  reporter.reset();
  layer.SetOpacity(1.0f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(reporter.reported());
}

// One reporter marked as "should_delete" should be deleted when
// reported.
TEST_F(TotalAnimationThroughputReporterTest, MAYBE_OnceReporterShouldDelete) {
  class DeleteTestReporter : public TotalAnimationThroughputReporter {
   public:
    DeleteTestReporter(Compositor* compositor,
                       ReportOnceCallback callback,
                       bool* deleted)
        : TotalAnimationThroughputReporter(compositor,
                                           std::move(callback),
                                           true),
          deleted_(deleted) {}
    ~DeleteTestReporter() override { *deleted_ = true; }

   private:
    bool* deleted_;
  };

  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(32));
  layer.SetAnimator(animator);

  // |reporter| keeps reporting as long as it is alive.
  bool reported = false;
  bool deleted = false;
  new DeleteTestReporter(
      compositor(),
      base::BindLambdaForTesting(
          [&](const cc::FrameSequenceMetrics::CustomReportData&) {
            reported = true;
          }),
      &deleted);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reported);
  EXPECT_TRUE(deleted);
}

}  // namespace ui
