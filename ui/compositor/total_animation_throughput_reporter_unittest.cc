// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/total_animation_throughput_reporter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/animation_throughput_reporter_test_base.h"
#include "ui/compositor/test/throughput_report_checker.h"

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
#define SANITIZER_ENABLED
#endif

namespace ui {
namespace {

#if !defined(SANITIZER_ENABLED)
// Returns the delta from current time to the (start + duration) time.
// This is used to compute how long it should wait from now to reach
// the `start + duration` time.
base::TimeDelta DeltaFromNowToTarget(const base::TimeTicks start,
                                     int duration) {
  return start + base::Milliseconds(duration) - base::TimeTicks::Now();
}
#endif

}  // namespace

using TotalAnimationThroughputReporterTest =
    AnimationThroughputReporterTestBase;

TEST_F(TotalAnimationThroughputReporterTest, SingleAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer.GetAnimator();
    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer.SetOpacity(1.0f);
  }
  Advance(base::Milliseconds(32));
  EXPECT_FALSE(checker.reported());
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests the stopping last animation will trigger the animation.
TEST_F(TotalAnimationThroughputReporterTest, StopAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(64));
    layer.SetOpacity(1.0f);
  }
  Advance(base::Milliseconds(32));
  EXPECT_FALSE(checker.reported());
  layer.GetAnimator()->StopAnimating();
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests the longest animation will trigger the report.
// TODO(crbug.com/1217783): Test is flaky.
TEST_F(TotalAnimationThroughputReporterTest, DISABLED_MultipleAnimations) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer1.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer1.SetOpacity(1.0f);
  }
  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(96));
    layer2.SetOpacity(1.0f);
  }
#if !defined(SANITIZER_ENABLED)
  auto start = base::TimeTicks::Now();
#endif
  Advance(base::Milliseconds(32));
  EXPECT_FALSE(checker.reported());

  // The following check may fail on sanitizer builds which
  // runs slwer.
#if !defined(SANITIZER_ENABLED)
  auto sixty_four_ms_from_start = DeltaFromNowToTarget(start, 64);
  ASSERT_TRUE(sixty_four_ms_from_start.is_positive());
  Advance(sixty_four_ms_from_start);
  EXPECT_FALSE(checker.reported());
#endif
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests the longest animation on a single layer will triger the report.
TEST_F(TotalAnimationThroughputReporterTest, MultipleAnimationsOnSingleLayer) {
  Layer layer;
  layer.SetOpacity(0.5f);
  layer.SetLayerBrightness(0.5f);
  root_layer()->Add(&layer);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer.SetOpacity(1.0f);
  }
  {
    LayerAnimator* animator = layer.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(96));
    layer.SetLayerBrightness(1.0f);
  }

  Advance(base::Milliseconds(64));
  EXPECT_FALSE(checker.reported());
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests adding new animation will extends the duration.
// TODO(crbug.com/1216715): Test is flaky.
TEST_F(TotalAnimationThroughputReporterTest,
       DISABLED_AddAnimationWhileAnimating) {
  Layer layer1;
  layer1.SetOpacity(0.5f);
  root_layer()->Add(&layer1);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer1.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer1.SetOpacity(1.0f);
  }
#if !defined(SANITIZER_ENABLED)
  base::TimeTicks start = base::TimeTicks::Now();
#endif
  Advance(base::Milliseconds(32));
  EXPECT_FALSE(checker.reported());

  // Add new animation while animating.
  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer2.SetOpacity(1.0f);
  }

  // The following check may fail on sanitizer builds which
  // runs slwer.
#if !defined(SANITIZER_ENABLED)
  // The animation time is extended by 32ms.
  auto sixty_four_ms_from_start = DeltaFromNowToTarget(start, 64);
  ASSERT_TRUE(sixty_four_ms_from_start.is_positive());
  Advance(sixty_four_ms_from_start);
  EXPECT_FALSE(checker.reported());
#endif

  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests removing last animation will call report callback.
TEST_F(TotalAnimationThroughputReporterTest, RemoveWhileAnimating) {
  auto layer1 = std::make_unique<Layer>();
  layer1->SetOpacity(0.5f);
  root_layer()->Add(layer1.get());

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  {
    LayerAnimator* animator = layer1->GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(100));
    layer1->SetOpacity(1.0f);
  }

  Layer layer2;
  layer2.SetOpacity(0.5f);
  root_layer()->Add(&layer2);

  {
    LayerAnimator* animator = layer2.GetAnimator();

    ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(base::Milliseconds(48));
    layer2.SetOpacity(1.0f);
  }
  Advance(base::Milliseconds(48));
  EXPECT_FALSE(checker.reported());
  layer1.reset();
  // Aborting will be processed in next frame.
  EXPECT_TRUE(checker.WaitUntilReported());
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
    settings.SetTransitionDuration(base::Milliseconds(96));
    layer.SetOpacity(1.0f);
  }
  Advance(base::Milliseconds(32));
  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());
  EXPECT_TRUE(reporter.IsMeasuringForTesting());
  EXPECT_TRUE(checker.WaitUntilReported());
}

// Tests the reporter is called multiple times for persistent animation.
TEST_F(TotalAnimationThroughputReporterTest, PersistedAnimation) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(48));
  layer.SetAnimator(animator);

  // |reporter| keeps reporting as long as it is alive.
  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(compositor(),
                                            checker.repeating_callback());

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  EXPECT_TRUE(checker.WaitUntilReported());

  // Report data for animation of opacity goes to 0.5.
  checker.reset();
  layer.SetOpacity(0.5f);
  EXPECT_TRUE(checker.WaitUntilReported());
}

namespace {

class ObserverChecker : public ui::CompositorObserver {
 public:
  ObserverChecker(ui::Compositor* compositor,
                  ui::CompositorObserver* reporter_observer)
      : reporter_observer_(reporter_observer) {
    EXPECT_TRUE(compositor->HasObserver(reporter_observer_));
    compositor->AddObserver(this);
  }
  ObserverChecker(const ObserverChecker&) = delete;
  ObserverChecker& operator=(const ObserverChecker&) = delete;
  ~ObserverChecker() override = default;

  // ui::CompositorObserver:
  void OnLastAnimationEnded(ui::Compositor* compositor) override {
    EXPECT_FALSE(compositor->HasObserver(reporter_observer_));
    compositor->RemoveObserver(this);
  }

 private:
  const raw_ptr<ui::CompositorObserver> reporter_observer_;
};

}  // namespace

// Make sure the once reporter is called only once.
TEST_F(TotalAnimationThroughputReporterTest, OnceReporter) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(32));
  layer.SetAnimator(animator);

  ThroughputReportChecker checker(this);
  TotalAnimationThroughputReporter reporter(
      compositor(), checker.once_callback(), /*should_delete=*/false);

  // Make sure the TotalAnimationThroughputReporter removes itself
  // from compositor as observer.
  ObserverChecker observer_checker(compositor(), &reporter);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  EXPECT_TRUE(checker.WaitUntilReported());

  // Report data for animation of opacity goes to 0.5.
  checker.reset();
  layer.SetOpacity(1.0f);
  Advance(base::Milliseconds(100));
  EXPECT_FALSE(checker.reported());
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
    raw_ptr<bool> deleted_;
  };

  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(32));
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
  run_loop.Run();
  EXPECT_TRUE(deleted);
}

TEST_F(TotalAnimationThroughputReporterTest, ThreadCheck) {
  Layer layer;
  layer.SetOpacity(0.5f);
  root_layer()->Add(&layer);

  // Set a persisted animator to |layer|.
  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(32));
  layer.SetAnimator(animator);

  ui::Compositor* c = compositor();

  ThroughputReportChecker checker(this);
  auto once_callback = checker.once_callback();
  ThroughputReportChecker::ReportOnceCallback callback =
      base::BindLambdaForTesting(
          [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
            // This call with fail if this is called on impl thread.
            c->ScheduleDraw();
            std::move(once_callback).Run(data);
          });

  TotalAnimationThroughputReporter reporter(c, std::move(callback),
                                            /*should_delete=*/false);

  // Report data for animation of opacity goes to 1.
  layer.SetOpacity(1.0f);
  EXPECT_TRUE(checker.WaitUntilReported());
}

}  // namespace ui
