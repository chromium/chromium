// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/types/display_constants.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

class MockCompositorObserver : public CompositorObserver {
 public:
  MOCK_METHOD2(OnCompositorVisibilityChanging,
               void(Compositor* compositor, bool visible));
  MOCK_METHOD2(OnCompositorVisibilityChanged,
               void(Compositor* compositor, bool visible));
};

class CompositorTest : public testing::Test {
 public:
  CompositorTest() = default;

  CompositorTest(const CompositorTest&) = delete;
  CompositorTest& operator=(const CompositorTest&) = delete;

  ~CompositorTest() override = default;

  void CreateCompositor() {
    compositor_ = std::make_unique<Compositor>(
        context_factories_->GetContextFactory()->AllocateFrameSinkId(),
        context_factories_->GetContextFactory(), CreateTaskRunner(),
        false /* enable_pixel_canvas */);
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }

  void SetUp() override {
    context_factories_ = std::make_unique<TestContextFactories>(false);
    CreateCompositor();
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
  void AdvanceBy(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// For tests that run on a real MessageLoop with real time.
class CompositorTestWithMessageLoop : public CompositorTest {
 public:
  CompositorTestWithMessageLoop()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ~CompositorTestWithMessageLoop() override = default;

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() override {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    return task_runner_;
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class TestCompositorAnimationObserver : public CompositorAnimationObserver {
 public:
  TestCompositorAnimationObserver() = default;
  TestCompositorAnimationObserver(const TestCompositorAnimationObserver&) =
      delete;
  TestCompositorAnimationObserver& operator=(
      const TestCompositorAnimationObserver&) = delete;
  ~TestCompositorAnimationObserver() override = default;

  // CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override {}
  void OnCompositingShuttingDown(Compositor* compositor) override {}

  void NotifyFailure() override { failed_ = true; }

  bool failed() const { return failed_; }

 private:
  bool failed_ = false;
};

}  // namespace

TEST_F(CompositorTestWithMockedTime, AnimationObserverBasic) {
  TestCompositorAnimationObserver test;
  compositor()->AddAnimationObserver(&test);

  test.Start();
  test.Check();
  AdvanceBy(base::Seconds(59));
  EXPECT_FALSE(test.failed());

  AdvanceBy(base::Seconds(2));
  test.Check();
  EXPECT_TRUE(test.failed());

  compositor()->RemoveAnimationObserver(&test);
}

TEST_F(CompositorTestWithMockedTime, AnimationObserverResetAfterResume) {
  TestCompositorAnimationObserver test;
  compositor()->AddAnimationObserver(&test);
  test.Start();
  test.Check();
  AdvanceBy(base::Seconds(59));
  EXPECT_TRUE(test.is_active_for_test());
  EXPECT_FALSE(test.failed());

  test_power_monitor_source_.Suspend();
  base::RunLoop().RunUntilIdle();
  AdvanceBy(base::Seconds(32));
  test_power_monitor_source_.Resume();
  base::RunLoop().RunUntilIdle();
  test.Check();
  EXPECT_TRUE(test.is_active_for_test());
  EXPECT_FALSE(test.failed());
  AdvanceBy(base::Seconds(29));
  test.Check();
  EXPECT_TRUE(test.is_active_for_test());
  EXPECT_FALSE(test.failed());

  AdvanceBy(base::Seconds(32));
  test.Check();
  EXPECT_FALSE(test.is_active_for_test());
  EXPECT_TRUE(test.failed());

  // Make sure another suspend/resume will not reactivate it.
  test_power_monitor_source_.Suspend();
  base::RunLoop().RunUntilIdle();
  test_power_monitor_source_.Resume();
  EXPECT_FALSE(test.is_active_for_test());
  EXPECT_TRUE(test.failed());

  compositor()->RemoveAnimationObserver(&test);
}

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
  // timebase, vsync interval, and max interval, and expect it to be set on the
  // context factory.
  SkM44 color_matrix;
  color_matrix.setRC(1, 1, 0.7f);
  color_matrix.setRC(2, 2, 0.4f);
  gfx::DisplayColorSpaces display_color_spaces(
      gfx::ColorSpace::CreateDisplayP3D65());
  display_color_spaces.SetSDRMaxLuminanceNits(1.f);
  base::TimeTicks vsync_timebase(base::TimeTicks::Now());
  base::TimeDelta vsync_interval(base::Milliseconds(250));
  base::TimeDelta max_vsync_interval(base::Milliseconds(500));
  compositor()->SetDisplayColorMatrix(color_matrix);
  compositor()->SetDisplayColorSpaces(display_color_spaces);
  compositor()->SetDisplayVSyncParameters(vsync_timebase, vsync_interval);
  compositor()->SetMaxVSyncAndVrr(
      max_vsync_interval, display::VariableRefreshRateState::kVrrEnabled);

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
  EXPECT_EQ(max_vsync_interval,
            context_factory->GetMaxVSyncInterval(compositor()));
  EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
            context_factory->GetVrrState(compositor()));

  // Simulate a lost context by releasing the output surface and setting it on
  // the compositor again. Expect that the same color matrix, color space, sdr
  // white level, vsync timebase, vsync interval, and max interval will be set
  // again on the context factory.
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
  EXPECT_EQ(max_vsync_interval,
            context_factory->GetMaxVSyncInterval(compositor()));
  EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
            context_factory->GetVrrState(compositor()));

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
        [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
          // This should not be called since the tracking is auto canceled.
          ADD_FAILURE();
        }));
    auto moved_tracker = std::move(tracker);
  }

  // Move a started instance and stop.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
          // May be called since Stop() is called.
        }));
    auto moved_tracker = std::move(tracker);
    EXPECT_TRUE(moved_tracker.Stop());
  }

  // Move a started instance and cancel.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
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
        [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
          // May be called since Stop() is called.
        }));
    EXPECT_TRUE(tracker.Stop());
    auto moved_tracker = std::move(tracker);
  }

  // Move a canceled instance.
  {
    auto tracker = compositor()->RequestNewThroughputTracker();
    tracker.Start(base::BindLambdaForTesting(
        [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
          // This should not be called since Cancel() is called.
          ADD_FAILURE();
        }));
    tracker.Cancel();
    auto moved_tracker = std::move(tracker);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// ui::ThroughputTracker is only supported on ChromeOS
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
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        EXPECT_GT(data.frames_expected_v3, 0u);
        EXPECT_GT(data.frames_expected_v3 - data.frames_dropped_v3, 0u);
        run_loop.Quit();
      }));

  // Generates a few frames after tracker starts to have some data collected.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  EXPECT_TRUE(tracker.Stop());

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
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        ADD_FAILURE() << "No report should happen";
      }));

  DestroyCompositor();

  // Stop() fails but no crash, no use-after-free and no report.
  EXPECT_FALSE(tracker.Stop());
}

TEST_F(CompositorTestWithMessageLoop, ThroughputTrackerCallbackStateChange) {
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
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        // The following Cancel() call should not DCHECK or crash.
        tracker.Cancel();

        // Starting another tracker should not DCHECK or crash.
        ThroughputTracker another_tracker =
            compositor()->RequestNewThroughputTracker();
        another_tracker.Start(base::DoNothing());

        run_loop.Quit();
      }));

  // Generates a few frames after tracker starts to have some data collected.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  EXPECT_TRUE(tracker.Stop());

  // Generates a few frames after tracker stops. Note the number of frames
  // must be at least two: one to trigger underlying cc::FrameSequenceTracker to
  // be scheduled for termination and one to report data.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  run_loop.Run();
}

TEST_F(CompositorTestWithMessageLoop, ThroughputTrackerInvoluntaryReport) {
  auto root_layer = std::make_unique<Layer>(ui::LAYER_SOLID_COLOR);
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10),
                                allocator.GetCurrentLocalSurfaceId());
  ASSERT_TRUE(compositor()->IsVisible());

  ThroughputTracker tracker = compositor()->RequestNewThroughputTracker();

  tracker.Start(base::BindLambdaForTesting(
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        ADD_FAILURE() << "No report should happen";
      }));

  // Generates a few frames after tracker starts to have some data collected.
  for (int i = 0; i < 5; ++i) {
    compositor()->ScheduleFullRedraw();
    DrawWaiterForTest::WaitForCompositingEnded(compositor());
  }

  // ReleaseAcceleratedWidget() destroys underlying cc::FrameSequenceTracker
  // and triggers reports before Stop(). Such reports are dropped.
  compositor()->SetVisible(false);
  compositor()->ReleaseAcceleratedWidget();

  // Stop() fails but no DCHECK or crash.
  EXPECT_FALSE(tracker.Stop());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40467610): Flaky on windows trybots
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

class LayerDelegateThatAddsDuringUpdateVisualState : public LayerDelegate {
 public:
  explicit LayerDelegateThatAddsDuringUpdateVisualState(Layer* parent)
      : parent_(*parent) {}

  bool update_visual_state_called() const {
    return update_visual_state_called_;
  }

  // LayerDelegate:
  void UpdateVisualState() override {
    added_layers_.push_back(std::make_unique<Layer>(ui::LAYER_SOLID_COLOR));
    parent_->Add(added_layers_.back().get());
    update_visual_state_called_ = true;
  }
  void OnPaintLayer(const PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  const raw_ref<Layer> parent_;
  std::vector<std::unique_ptr<Layer>> added_layers_;
  bool update_visual_state_called_ = false;
};

TEST_F(CompositorTestWithMessageLoop, AddLayerDuringUpdateVisualState) {
  std::unique_ptr<Layer> root_layer =
      std::make_unique<Layer>(ui::LAYER_SOLID_COLOR);
  std::unique_ptr<Layer> child_layer =
      std::make_unique<Layer>(ui::LAYER_TEXTURED);
  std::unique_ptr<Layer> child_layer2 =
      std::make_unique<Layer>(ui::LAYER_SOLID_COLOR);
  LayerDelegateThatAddsDuringUpdateVisualState child_layer_delegate(
      root_layer.get());
  child_layer->set_delegate(&child_layer_delegate);
  root_layer->Add(child_layer.get());
  root_layer->Add(child_layer2.get());

  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10),
                                allocator.GetCurrentLocalSurfaceId());
  ASSERT_TRUE(compositor()->IsVisible());
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  EXPECT_TRUE(child_layer_delegate.update_visual_state_called());
  compositor()->SetRootLayer(nullptr);
}

TEST_F(CompositorTestWithMessageLoop, CompositorVisibilityChanges) {
  testing::StrictMock<MockCompositorObserver> observer;
  compositor()->AddObserver(&observer);

  EXPECT_CALL(observer, OnCompositorVisibilityChanging(compositor(), false))
      .Times(1);
  EXPECT_CALL(observer, OnCompositorVisibilityChanged(compositor(), false))
      .Times(1);
  compositor()->SetVisible(false);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnCompositorVisibilityChanging(compositor(), true))
      .Times(1);
  EXPECT_CALL(observer, OnCompositorVisibilityChanged(compositor(), true))
      .Times(1);
  compositor()->SetVisible(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify no calls if visibility isn't changed.
  EXPECT_CALL(observer, OnCompositorVisibilityChanging(compositor(), _))
      .Times(0);
  EXPECT_CALL(observer, OnCompositorVisibilityChanged(compositor(), _))
      .Times(0);
  compositor()->SetVisible(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  compositor()->RemoveObserver(&observer);
}

}  // namespace ui
