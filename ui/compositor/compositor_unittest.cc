// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/compositor/test/test_context_factories.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

class CompositorTest : public testing::Test {
 public:
  CompositorTest() = default;
  ~CompositorTest() override = default;

  void SetUp() override {
    context_factories_ = std::make_unique<TestContextFactories>(false);

    compositor_ = std::make_unique<Compositor>(
        context_factories_->GetContextFactory()->AllocateFrameSinkId(),
        context_factories_->GetContextFactory(), CreateTaskRunner(),
        false /* enable_pixel_canvas */);
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }

  void TearDown() override {
    compositor_.reset();
    context_factories_.reset();
  }

  void DestroyCompositor() { compositor_.reset(); }

 protected:
  virtual scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() = 0;

  Compositor* compositor() { return compositor_.get(); }

 private:
  std::unique_ptr<TestContextFactories> context_factories_;
  std::unique_ptr<Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTest);
};

// For tests that control time.
class CompositorTestWithMockedTime : public CompositorTest {
 protected:
  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() override {
    task_runner_ = new base::TestMockTimeTaskRunner;
    return task_runner_;
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

// For tests that run on a real MessageLoop with real time.
class CompositorTestWithMessageLoop : public CompositorTest {
 public:
  CompositorTestWithMessageLoop()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ~CompositorTestWithMessageLoop() override = default;

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() override {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    return task_runner_;
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

TEST_F(CompositorTestWithMessageLoop, ShouldUpdateDisplayProperties) {
  auto root_layer = std::make_unique<Layer>(ui::LAYER_SOLID_COLOR);
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10),
                                allocator.GetCurrentLocalSurfaceId());
  ASSERT_TRUE(compositor()->IsVisible());

  // Set a non-identity color matrix, color space, sdr white level, vsync
  // timebase and vsync interval, and expect it to be set on the context
  // factory.
  SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
  color_matrix.set(1, 1, 0.7f);
  color_matrix.set(2, 2, 0.4f);
  gfx::DisplayColorSpaces display_color_spaces(
      gfx::ColorSpace::CreateDisplayP3D65());
  display_color_spaces.SetSDRWhiteLevel(1.f);
  base::TimeTicks vsync_timebase(base::TimeTicks::Now());
  base::TimeDelta vsync_interval(base::TimeDelta::FromMilliseconds(250));
  compositor()->SetDisplayColorMatrix(color_matrix);
  compositor()->SetDisplayColorSpaces(display_color_spaces);
  compositor()->SetDisplayVSyncParameters(vsync_timebase, vsync_interval);

  InProcessContextFactory* context_factory =
      static_cast<InProcessContextFactory*>(compositor()->context_factory());
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  EXPECT_EQ(color_matrix, context_factory->GetOutputColorMatrix(compositor()));
  EXPECT_EQ(display_color_spaces,
            context_factory->GetDisplayColorSpaces(compositor()));
  EXPECT_EQ(vsync_timebase,
            context_factory->GetDisplayVSyncTimeBase(compositor()));
  EXPECT_EQ(vsync_interval,
            context_factory->GetDisplayVSyncTimeInterval(compositor()));

  // Simulate a lost context by releasing the output surface and setting it on
  // the compositor again. Expect that the same color matrix, color space, sdr
  // white level, vsync timebase and vsync interval will be set again on the
  // context factory.
  context_factory->ResetDisplayOutputParameters(compositor());
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  EXPECT_EQ(color_matrix, context_factory->GetOutputColorMatrix(compositor()));
  EXPECT_EQ(display_color_spaces,
            context_factory->GetDisplayColorSpaces(compositor()));
  EXPECT_EQ(vsync_timebase,
            context_factory->GetDisplayVSyncTimeBase(compositor()));
  EXPECT_EQ(vsync_interval,
            context_factory->GetDisplayVSyncTimeInterval(compositor()));
  compositor()->SetRootLayer(nullptr);
}

TEST_F(CompositorTestWithMockedTime,
       ReleaseWidgetWithOutputSurfaceNeverCreated) {
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
}

TEST_F(CompositorTestWithMessageLoop, MoveThroughputTracker) {
  // Move a not started instance.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    auto moved_tracker = std::move(tracker);
  }

  // Move a started instance.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
          // This should not be called since the tracking is auto canceled.
          ADD_FAILURE();
        }));
    auto moved_tracker = std::move(tracker);
  }

  // Move a started instance and stop.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
          // May be called since Stop() is called.
        }));
    auto moved_tracker = std::move(tracker);
    moved_tracker.Stop();
  }

  // Move a started instance and cancel.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
          // This should not be called since Cancel() is called.
          ADD_FAILURE();
        }));
    auto moved_tracker = std::move(tracker);
    moved_tracker.Cancel();
  }

  // Move a stopped instance.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
          // May be called since Stop() is called.
        }));
    tracker.Stop();
    auto moved_tracker = std::move(tracker);
  }

  // Move a canceled instance.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
          // This should not be called since Cancel() is called.
          ADD_FAILURE();
        }));
    tracker.Cancel();
    auto moved_tracker = std::move(tracker);
  }
}

TEST_F(CompositorTestWithMessageLoop, ThroughputTracker) {
  auto root_layer = std::make_unique<Layer>(ui::LAYER_SOLID_COLOR);
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10),
                                allocator.GetCurrentLocalSurfaceId());
  ASSERT_TRUE(compositor()->IsVisible());

  ThroughputTracker tracker = compositor()->RequestNewThroughputTracker();

  base::RunLoop run_loop;
  tracker.Start(base::BindLambdaForTesting(
      [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
        EXPECT_GT(throughput.frames_expected, 0u);
        EXPECT_GT(throughput.frames_produced, 0u);
        run_loop.Quit();
      }));

  // Generates a few frames after tracker starts to have some data collected.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  tracker.Stop();

  // Generates a few frames after tracker stops. Note the number of frames
  // must be at least two: one to trigger underlying cc::FrameSequenceTracker to
  // be scheduled for termination and one to report data.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  run_loop.Run();
}

TEST_F(CompositorTestWithMessageLoop, ThroughputTrackerOutliveCompositor) {
  auto tracker = compositor()->RequestNewThroughputTracker();
  tracker.Start(base::BindLambdaForTesting(
      [&](cc::FrameSequenceMetrics::ThroughputData throughput) {
        ADD_FAILURE() << "No report should happen";
      }));

  DestroyCompositor();

  // No crash, no use-after-free and no report.
  tracker.Stop();
}

#if defined(OS_WIN)
// TODO(crbug.com/608436): Flaky on windows trybots
#define MAYBE_CreateAndReleaseOutputSurface \
  DISABLED_CreateAndReleaseOutputSurface
#else
#define MAYBE_CreateAndReleaseOutputSurface CreateAndReleaseOutputSurface
#endif
TEST_F(CompositorTestWithMessageLoop, MAYBE_CreateAndReleaseOutputSurface) {
  std::unique_ptr<Layer> root_layer(new Layer(ui::LAYER_SOLID_COLOR));
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10),
                                allocator.GetCurrentLocalSurfaceId());
  ASSERT_TRUE(compositor()->IsVisible());
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetRootLayer(nullptr);
}

}  // namespace ui
