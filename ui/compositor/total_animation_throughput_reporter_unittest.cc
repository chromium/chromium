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

namespace ui {
namespace {

class TestReporter : public TotalAnimationThroughputReporter {
 public:
  explicit TestReporter(AnimationThroughputReporterTestBase* test_base)
      : ui::TotalAnimationThroughputReporter(
            test_base->compositor(),
            base::BindRepeating(&TestReporter::Reported,
                                base::Unretained(this))),
        test_base_(test_base) {}
  TestReporter(AnimationThroughputReporterTestBase* test_base,
               bool should_delete)
      : ui::TotalAnimationThroughputReporter(
            test_base->compositor(),
            base::BindOnce(&TestReporter::Reported, base::Unretained(this)),
            should_delete),
        test_base_(test_base) {}

  TestReporter(const TestReporter&) = delete;
  TestReporter& operator=(const TestReporter&) = delete;
  ~TestReporter() override = default;

  void AdvanceUntilReported(const base::TimeDelta& delta) {
    DCHECK(!reported_);
    test_base_->Advance(delta);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Non ash-chrome platform uses native event loop which doesn't work well
    // with mock time, so we still need to run the event loop.
    if (!reported_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
#endif
  }

  bool reported() const { return reported_; }

  void reset() { reported_ = false; }

 private:
  void Reported(const cc::FrameSequenceMetrics::CustomReportData&) {
    reported_ = true;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    if (run_loop_)
      run_loop_->Quit();
#endif
  }

  AnimationThroughputReporterTestBase* test_base_;
  bool reported_ = false;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<base::RunLoop> run_loop_;
#endif
};

}  // namespace

using TotalAnimationThroughputReporterTest =
    AnimationThroughputReporterTestBase;

TEST_F(TotalAnimationThroughputReporterTest, SingleAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(this);
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(48));
    layer.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(32));
  EXPECT_TRUE(reporter.reported());
}

// Tests the stopping last animation will trigger the animation.
TEST_F(TotalAnimationThroughputReporterTest, StopAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(this);
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(64));
    layer.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(32));
  EXPECT_FALSE(reporter.reported());
  layer.GetAnimator()->StopAnimating();
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(32));
  EXPECT_TRUE(reporter.reported());
}

// Tests the longest animation will trigger the report.
TEST_F(TotalAnimationThroughputReporterTest, MultipleAnimations) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  TestReporter reporter(this);
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
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(200));
  EXPECT_TRUE(reporter.reported());
}

// Tests the longest animation on a single layer will triger the report.
TEST_F(TotalAnimationThroughputReporterTest, MultipleAnimationsOnSingleLayer) {
  Layer layer;
  layer.SetOpacity(0.5f);
  layer.SetLayerBrightness(0.5f);
  root_layer()->Add(&layer);

  TestReporter reporter(this);
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
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(48));
  EXPECT_TRUE(reporter.reported());
}

// Tests adding new animation will extends the duration.
TEST_F(TotalAnimationThroughputReporterTest, AddAnimationWhileAnimating) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  TestReporter reporter(this);
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

  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(32));
  EXPECT_TRUE(reporter.reported());
}

// Tests removing last animation will call report callback.
TEST_F(TotalAnimationThroughputReporterTest, RemoveWhileAnimating) {
  auto layer1 = std::make_unique<Layer>();
  layer1->SetOpacity(0.5f);
  root_layer()->Add(layer1.get());

  TestReporter reporter(this);
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
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(16));
  EXPECT_TRUE(reporter.reported());
}

// Make sure the reporter can start measuring even if the animation
// has started.
TEST_F(TotalAnimationThroughputReporterTest, StartWhileAnimating) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(96));
    layer.SetOpacity(1.0f);
  }
  Advance(base::TimeDelta::FromMilliseconds(32));
  TestReporter reporter(this);
  EXPECT_TRUE(reporter.IsMeasuringForTesting());
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());
}

// Tests the reporter is called multiple times for persistent animation.
TEST_F(TotalAnimationThroughputReporterTest, PersistedAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(48));
  layer.SetAnimator(animator);

  // |reporter| keeps reporting as long as it is alive.
  TestReporter reporter(this);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());

  // Report data for animation of opacity goes to 0.5.
  reporter.reset();
  layer.SetOpacity(0.5f);
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());
}

// Make sure the once reporter is called only once.
TEST_F(TotalAnimationThroughputReporterTest, OnceReporter) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator =
      new LayerAnimator(base::TimeDelta::FromMilliseconds(32));
  layer.SetAnimator(animator);

  TestReporter reporter(this, /*should_delete=*/false);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  reporter.AdvanceUntilReported(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(reporter.reported());

  // Report data for animation of opacity goes to 0.5.
  reporter.reset();
  layer.SetOpacity(1.0f);
  Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(reporter.reported());
}

// One reporter marked as "should_delete" should be deleted when
// reported.
TEST_F(TotalAnimationThroughputReporterTest, OnceReporterShouldDelete) {
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
  base::RunLoop run_loop;

  bool deleted = false;
  new DeleteTestReporter(
      compositor(),
      base::BindLambdaForTesting(
          [&](const cc::FrameSequenceMetrics::CustomReportData&) {
            run_loop.Quit();
          }),
      &deleted);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Advance(base::TimeDelta::FromMilliseconds(48));
  EXPECT_FALSE(run_loop.running());
#else
  // Non ash-chrome platform uses native event loop which doesn't work
  // with mock time, so we need to run more the event loop.
  run_loop.Run();
#endif
  EXPECT_TRUE(deleted);
}

}  // namespace ui
