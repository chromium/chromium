// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_whole_screen_move_loop.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point.h"

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

}  // namespace ui
