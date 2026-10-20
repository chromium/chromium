// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_whole_screen_move_loop.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace {

class TestPlatformEventSource : public PlatformEventSource {
 public:
  TestPlatformEventSource() = default;
  ~TestPlatformEventSource() override = default;
};

class TestMoveLoopDelegate : public X11MoveLoopDelegate {
 public:
  TestMoveLoopDelegate() = default;
  ~TestMoveLoopDelegate() override = default;

  // X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override {}
  void OnMouseReleased() override {}
  void OnMoveLoopEnded() override {
    loop_.reset();
  }

  void Init(std::unique_ptr<X11WholeScreenMoveLoop> loop) {
    loop_ = std::move(loop);
  }

  bool has_loop() const { return !!loop_; }

 private:
  std::unique_ptr<X11WholeScreenMoveLoop> loop_;
};

}  // namespace

TEST(X11WholeScreenMoveLoopTest, EndMoveLoopSurvivesSelfDeletion) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  TestPlatformEventSource event_source;

  TestMoveLoopDelegate delegate;
  auto move_loop = std::make_unique<X11WholeScreenMoveLoop>(&delegate);
  X11WholeScreenMoveLoop* move_loop_ptr = move_loop.get();
  delegate.Init(std::move(move_loop));

  // Post a task to end the move loop. This task will run after the nested
  // run loop starts, which synchronously invokes delegate->OnMoveLoopEnded()
  // and deletes the loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&X11WholeScreenMoveLoop::EndMoveLoop,
                                base::Unretained(move_loop_ptr)));

  bool run_result = move_loop_ptr->RunMoveLoop(
      /*can_grab_pointer=*/false, nullptr, nullptr, base::DoNothing());

  EXPECT_FALSE(delegate.has_loop());
  EXPECT_FALSE(run_result);
}

namespace {

// Ends the move loop when it reports a release, like the real delegates do.
class EndOnReleaseDelegate : public X11MoveLoopDelegate {
 public:
  // X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override {}
  void OnMouseReleased() override { loop_->EndMoveLoop(); }
  void OnMoveLoopEnded() override {}

  void set_loop(X11WholeScreenMoveLoop* loop) { loop_ = loop; }

 private:
  raw_ptr<X11WholeScreenMoveLoop> loop_ = nullptr;
};

}  // namespace

// The loop consumes the touch release that ends a touch drag, so it must report
// the touch sequence to its owner, which ends it once the loop returns.
TEST(X11WholeScreenMoveLoopTest, ReportsConsumedTouchRelease) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  TestPlatformEventSource event_source;

  EndOnReleaseDelegate delegate;
  X11WholeScreenMoveLoop move_loop(&delegate);
  delegate.set_loop(&move_loop);

  // Events dispatched outside the loop are not consumed.
  TouchEvent early_release(EventType::kTouchReleased, gfx::PointF(1, 1),
                           gfx::PointF(1, 1), base::TimeTicks::Now(),
                           PointerDetails(EventPointerType::kTouch, 3));
  EXPECT_EQ(static_cast<uint32_t>(POST_DISPATCH_PERFORM_DEFAULT),
            move_loop.DispatchEvent(&early_release));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&move_loop]() {
        TouchEvent release(EventType::kTouchReleased, gfx::PointF(10, 20),
                           gfx::PointF(10, 20), base::TimeTicks::Now(),
                           PointerDetails(EventPointerType::kTouch, 5));
        EXPECT_EQ(static_cast<uint32_t>(POST_DISPATCH_NONE),
                  move_loop.DispatchEvent(&release));
      }));

  move_loop.RunMoveLoop(/*can_grab_pointer=*/false, nullptr, nullptr,
                        base::DoNothing());

  auto consumed = move_loop.TakeConsumedTouches();
  ASSERT_EQ(1u, consumed.size());
  EXPECT_EQ(5, consumed[0].id);
  EXPECT_EQ(gfx::PointF(10, 20), consumed[0].location);
  EXPECT_TRUE(move_loop.TakeConsumedTouches().empty());
}

}  // namespace ui
