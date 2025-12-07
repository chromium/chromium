// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/host_begin_frame_observer.h"

#include <optional>
#include <utility>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace ui {
namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

class MockSimpleBeginFrameObserver
    : public HostBeginFrameObserver::SimpleBeginFrameObserver {
 public:
  MOCK_METHOD(void,
              OnBeginFrame,
              (base::TimeTicks,
               base::TimeDelta,
               std::optional<base::TimeTicks>),
              (override));
  MOCK_METHOD(void, OnBeginFrameSourceShuttingDown, (), (override));
};

viz::BeginFrameArgs CreateBeginFrameArgs(base::TimeTicks frame_time) {
  return viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, viz::BeginFrameArgs::kStartingFrameNumber,
      frame_time, base::TimeTicks::Max(), kVsyncInterval,
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
}

// Test fixture for LayerOwner tests that require a ui::Compositor.
class HostBeginFrameObserverTest : public testing::Test {
 public:
  HostBeginFrameObserverTest() = default;

  HostBeginFrameObserverTest(const HostBeginFrameObserverTest&) = delete;
  HostBeginFrameObserverTest& operator=(const HostBeginFrameObserverTest&) =
      delete;

  ~HostBeginFrameObserverTest() override = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  MockSimpleBeginFrameObserver mock_observer_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ui::HostBeginFrameObserver::SimpleBeginFrameObserverList observer_list_;
  HostBeginFrameObserver host_observer_{
      observer_list_, base::SingleThreadTaskRunner::GetCurrentDefault()};
};

void HostBeginFrameObserverTest::SetUp() {
  observer_list_.AddObserver(&mock_observer_);
}

void HostBeginFrameObserverTest::TearDown() {
  observer_list_.RemoveObserver(&mock_observer_);
}

}  // namespace

TEST_F(HostBeginFrameObserverTest, MissedFrame) {
  base::TimeTicks frame_time = base::TimeTicks::Now() - base::Milliseconds(15);
  viz::BeginFrameArgs args = CreateBeginFrameArgs(frame_time);
  args.type = viz::BeginFrameArgs::MISSED;

  EXPECT_CALL(mock_observer_, OnBeginFrame(_, _, _)).Times(0);

  host_observer_.OnStandaloneBeginFrame(args);
}

TEST_F(HostBeginFrameObserverTest, FrameYougerThanVsyncInterval) {
  {
    base::TimeTicks frame_time1 =
        base::TimeTicks::Now() - base::Milliseconds(15);
    viz::BeginFrameArgs args1 = CreateBeginFrameArgs(frame_time1);

    EXPECT_CALL(mock_observer_, OnBeginFrame(frame_time1, kVsyncInterval,
                                             std::optional<base::TimeTicks>{}))
        .Times(1);

    host_observer_.OnStandaloneBeginFrame(args1);
  }

  // Check that the behavior repeats.
  {
    task_environment_.FastForwardBy(base::Milliseconds(100));
    base::TimeTicks frame_time2 =
        base::TimeTicks::Now() - base::Milliseconds(15);
    viz::BeginFrameArgs args2 = CreateBeginFrameArgs(frame_time2);
    EXPECT_CALL(mock_observer_, OnBeginFrame(frame_time2, kVsyncInterval,
                                             std::optional<base::TimeTicks>{}))
        .Times(1);
    host_observer_.OnStandaloneBeginFrame(args2);
  }
}

TEST_F(HostBeginFrameObserverTest, FrameOlderThanVsyncInterval) {
  {
    base::TimeTicks frame_time1 =
        base::TimeTicks::Now() - base::Milliseconds(17);
    viz::BeginFrameArgs args1 = CreateBeginFrameArgs(frame_time1);

    EXPECT_CALL(mock_observer_, OnBeginFrame(_, _, _)).Times(0);

    host_observer_.OnStandaloneBeginFrame(args1);

    EXPECT_CALL(mock_observer_, OnBeginFrame(frame_time1, kVsyncInterval,
                                             std::optional<base::TimeTicks>{}))
        .Times(1);

    task_environment_.FastForwardBy(base::Milliseconds(100));
  }

  // Check that things go back to normal afterwards.
  {
    base::TimeTicks frame_time2 =
        base::TimeTicks::Now() - base::Milliseconds(15);
    viz::BeginFrameArgs args2 = CreateBeginFrameArgs(frame_time2);
    EXPECT_CALL(mock_observer_, OnBeginFrame(frame_time2, kVsyncInterval,
                                             std::optional<base::TimeTicks>{}))
        .Times(1);
    host_observer_.OnStandaloneBeginFrame(args2);
  }
}

TEST_F(HostBeginFrameObserverTest,
       FrameOlderThanVsyncIntervalFollowedByMoreFrames) {
  {
    base::TimeTicks frame_time1 =
        base::TimeTicks::Now() - base::Milliseconds(17);
    base::TimeTicks frame_time2 =
        base::TimeTicks::Now() - base::Milliseconds(15);
    base::TimeTicks frame_time3 =
        base::TimeTicks::Now() - base::Milliseconds(13);
    viz::BeginFrameArgs args1 = CreateBeginFrameArgs(frame_time1);
    viz::BeginFrameArgs args2 = CreateBeginFrameArgs(frame_time2);
    viz::BeginFrameArgs args3 = CreateBeginFrameArgs(frame_time3);

    EXPECT_CALL(mock_observer_, OnBeginFrame(_, _, _)).Times(0);

    host_observer_.OnStandaloneBeginFrame(args1);
    host_observer_.OnStandaloneBeginFrame(args2);
    host_observer_.OnStandaloneBeginFrame(args3);

    EXPECT_CALL(mock_observer_,
                OnBeginFrame(frame_time3, kVsyncInterval,
                             std::optional<base::TimeTicks>{frame_time1}))
        .Times(1);

    task_environment_.FastForwardBy(base::Milliseconds(100));
  }

  // Check that things go back to normal afterwards.
  {
    base::TimeTicks frame_time4 =
        base::TimeTicks::Now() - base::Milliseconds(15);
    viz::BeginFrameArgs args4 = CreateBeginFrameArgs(frame_time4);
    EXPECT_CALL(mock_observer_, OnBeginFrame(frame_time4, kVsyncInterval,
                                             std::optional<base::TimeTicks>{}))
        .Times(1);
    host_observer_.OnStandaloneBeginFrame(args4);
  }
}

}  // namespace ui
