// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/native_widget_ns_window_fullscreen_controller.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remote_cocoa::test {

using testing::_;
using testing::Invoke;
using testing::Return;

class MockClient : public NativeWidgetNSWindowFullscreenController::Client {
 public:
  MOCK_METHOD1(FullscreenControllerTransitionStart, void(bool));
  MOCK_METHOD1(FullscreenControllerTransitionComplete, void(bool));
  MOCK_METHOD3(FullscreenControllerSetFrame,
               void(const gfx::Rect&,
                    bool animate,
                    base::OnceCallback<void()> animation_complete));

  MOCK_METHOD0(FullscreenControllerToggleFullscreen, void());
  MOCK_METHOD0(FullscreenControllerCloseWindow, void());
  MOCK_CONST_METHOD0(FullscreenControllerGetDisplayId, int64_t());
  MOCK_CONST_METHOD1(FullscreenControllerGetFrameForDisplay,
                     gfx::Rect(int64_t display_id));
  MOCK_CONST_METHOD0(FullscreenControllerGetFrame, gfx::Rect());
};

class MacFullscreenControllerTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen()).Times(0);
    EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(_, _, _)).Times(0);
    EXPECT_CALL(mock_client_, FullscreenControllerCloseWindow()).Times(0);
  }

 protected:
  const gfx::Rect kWindowRect{1, 2, 3, 4};
  const int64_t kDisplay0Id = 0;
  const int64_t kDisplay1Id = 1;
  const gfx::Rect kDisplay0Frame{9, 10, 11, 12};
  const gfx::Rect kDisplay1Frame{13, 14, 15, 16};

  MockClient mock_client_;
  NativeWidgetNSWindowFullscreenController controller_{&mock_client_};
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Fake implementation of FullscreenControllerSetFrame
// which executes the given callback immediately.
void FullscreenControllerSetFrameFake(const gfx::Rect&,
                                      bool,
                                      base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

// Simple enter-and-exit fullscreen via the green traffic light button.
TEST_F(MacFullscreenControllerTest, SimpleUserInitiated) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // The controller should do nothing, and wait until it receives
  // OnWindowDidEnterFullscreen.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Calling EnterFullscreen inside the transition will do nothing.
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Calling EnterFullscreen while fullscreen should do nothing.
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Calling EnterFullscreen specifying the display that we are currently on
  // will also be a no-op.
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillOnce(Return(kDisplay0Id));
  controller_.EnterFullscreen(kDisplay0Id);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen via the green traffic light.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Calling ExitFullscreen inside the transition will do nothing.
  controller_.ExitFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Complete the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  controller_.OnWindowDidExitFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
  task_environment_.RunUntilIdle();
}

// Simple enter-and-exit fullscreen programmatically.
TEST_F(MacFullscreenControllerTest, SimpleProgrammatic) {
  // Enter fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // The call to ToggleFullscreen is sent via a posted task, so it shouldn't
  // happen until after the run loop is pumped. The function
  // OnWindowWillEnterFullscreen is called from within ToggleFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Complete the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());

  // Exit fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.ExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Make ToggleFullscreen happen, and invoke OnWindowWillExitFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Complete the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  controller_.OnWindowDidExitFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  task_environment_.RunUntilIdle();
}

// A transition that fails to enter fullscreen.
TEST_F(MacFullscreenControllerTest, FailEnterFullscreenSimple) {
  // Enter fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();

  // Fail the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  controller_.OnWindowDidExitFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

// A transition that fails to exit fullscreen.
TEST_F(MacFullscreenControllerTest, FailExitFullscreen) {
  // Enter fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Start to exit fullscreen, through OnWindowWillExitFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.ExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Fail the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());
}

// A simple cross-screen transition.
TEST_F(MacFullscreenControllerTest, SimpleCrossScreen) {
  // Enter fullscreen on display 1, from display 0.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // This will trigger a call to MoveToTargetDisplayThenToggleFullscreen, even
  // though we are not actually moving displays. It will also then post a task
  // to ToggleFullscreen (because RunUntilIdle will pick up that posted task).
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .Times(1)
      .WillOnce(Return(kDisplay1Frame));
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen on the new display.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Re-entering fullscreen on our current display should be a no-op.
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillOnce(Return(kDisplay1Id));
  controller_.EnterFullscreen(kDisplay1Id);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen. This will post a task to restore bounds.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.ExitFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Let the run loop run, it will restore the bounds.
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

// A cross-screen transition after having entered fullscreen.
TEST_F(MacFullscreenControllerTest, CrossScreenFromFullscreen) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Enter fullscreen on a different display.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillRepeatedly(Return(kDisplay0Id));
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Execute the task posted to ToggleFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to windowed. This will post a task to move to the
  // target display's frame.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .Times(1)
      .WillOnce(Return(kDisplay1Frame));
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Execute the posted task to set the new frame. This will also re-toggle
  // fullscreen in the RunUntilIdle.
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen. This will restore bounds.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.ExitFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  controller_.OnWindowDidExitFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

// Fail a cross-screen transition when exiting fullscreen.
TEST_F(MacFullscreenControllerTest, CrossScreenFromFullscreenFailExit) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Attempt to enter fullscreen on a different display. Get as far as toggling
  // fullscreen to get back to windowed mode.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillRepeatedly(Return(kDisplay0Id));
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Fail the transition back to windowed. This should just leave us as being
  // fullscreen. It will issue the TransitionComplete notification indicating
  // that the transition is over, and that we're in fullscreen mode (no mention
  // of which display).
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // No further tasks should run.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());
}

// Fail a cross-screen transition when entering fullscreen.
TEST_F(MacFullscreenControllerTest, CrossScreenFromFullscreenFailEnter) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Enter fullscreen on a different display. Get as far as toggling fullscreen
  // to get back to windowed mode, moving the window, and then toggling
  // fullscreen on the new display.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillRepeatedly(Return(kDisplay0Id));
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .Times(1)
      .WillOnce(Return(kDisplay1Frame));
  controller_.OnWindowDidExitFullscreen();
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Fail the transition to fullscreen mode. This will cause us to post tasks
  // to stay in windowed mode.
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // This will cause us to restore the window's original frame, and then declare
  // the transition complete, with the final state after transition as being
  // windowed.
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

// Be instructed to exit fullscreen while entering fullscreen.
TEST_F(MacFullscreenControllerTest, ExitWhileEntering) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());

  // Call ExitFullscreen inside the transition.
  controller_.ExitFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen. This will report that it is still
  // in transition to windowed.
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // The above call will have posted a ToggleFullscreen task.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();

  // Complete the transition to windowed mode.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  controller_.OnWindowDidExitFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Nothing more should happen.
  task_environment_.RunUntilIdle();
}

// Be instructed to enter fullscreen while exiting fullscreen.
TEST_F(MacFullscreenControllerTest, EnterWhileExiting) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Call EnterFullscreen inside the transition.
  controller_.EnterFullscreen(display::kInvalidDisplayId);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to windowed mode. This will report that it is
  // still in transition to fullscreen.
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // The above call will have posted a ToggleFullscreen task.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen mode.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Nothing more should happen.
  task_environment_.RunUntilIdle();
}

// Be instructed to enter fullscreen on a different screen, while exiting
// fullscreen.
TEST_F(MacFullscreenControllerTest, EnterCrossScreenWhileExiting) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Call EnterFullscreen inside the transition.
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to windowed mode. This will report that it is
  // still in transition to fullscreen.
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // The above call will have posted a MoveToTargetDisplayThenToggleFullscreen
  // task, which will then post a ToggleFullscreen task.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .Times(1)
      .WillOnce(Return(kDisplay1Frame));
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen mode.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Nothing more should happen.
  task_environment_.RunUntilIdle();
}

// Be instructed to enter fullscreen on a different screen, while entering
// fullscreen.
TEST_F(MacFullscreenControllerTest, EnterCrossScreenWhileEntering) {
  // Enter fullscreen the way that clicking the green traffic light does it.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .Times(1)
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.OnWindowWillEnterFullscreen();
  task_environment_.RunUntilIdle();

  // Call EnterFullscreen inside the transition.
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the original fullscreen transition. This will check to see if
  // the display we are on is the display we wanted to be on. Seeing that it
  // isn't, it will post a task to exit fullscreen before moving to the correct
  // display.
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillOnce(Return(kDisplay0Id));
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // The above will have posted a task to ToggleFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillExitFullscreen));
  task_environment_.RunUntilIdle();

  // Complete the transition to windowed mode. This will once again check to see
  // if the display we are on is the display we wanted to be on. Seeing that it
  // isn't, it will move to the correct display and toggle fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerGetDisplayId())
      .WillOnce(Return(kDisplay0Id));
  controller_.OnWindowDidExitFullscreen();
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .Times(1)
      .WillOnce(Return(kDisplay1Frame));
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Complete the transition to windowed mode. The frame frame should be reset.
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

// Be instructed to enter fullscreen while restoring the windowed frame.
TEST_F(MacFullscreenControllerTest, EnterCrossScreenWhileRestoring) {
  // Enter fullscreen on display 1, from display 0.
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrame())
      .WillOnce(Return(kWindowRect));
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(true)).Times(1);
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // This will trigger a call to MoveToTargetDisplayThenToggleFullscreen, even
  // though we are not actually moving displays. It will also then post a task
  // to ToggleFullscreen (because RunUntilIdle will pick up that posted task).
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .WillOnce(Return(kDisplay1Frame));
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen on the new display.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen. This will post a task to restore the windowed frame.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
  task_environment_.RunUntilIdle();
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Enter fullscreen on display 1 again, while restoring the windowed frame.
  // This does not call FullscreenControllerGetFrame() to update the cached
  // windowed frame, since the window is still in transition. It also will not
  // call FullscreenControllerTransitionComplete(false) to signal the successful
  // exit, since that might ambiguously signal that entering fullscreen failed.
  // It also will not call FullscreenControllerTransitionStart(true), since the
  // window is already in a transition.
  controller_.EnterFullscreen(kDisplay1Id);
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Let the run loop run, the task to restore the windowed frame will execute
  // before handling the new pending state from EnterFullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  // This will trigger a call to MoveToTargetDisplayThenToggleFullscreen, even
  // though we are not actually moving displays. It will also then post a task
  // to ToggleFullscreen (because RunUntilIdle will pick up that posted task).
  EXPECT_CALL(mock_client_, FullscreenControllerGetFrameForDisplay(kDisplay1Id))
      .WillOnce(Return(kDisplay1Frame));
  EXPECT_CALL(mock_client_,
              FullscreenControllerSetFrame(kDisplay1Frame, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerToggleFullscreen())
      .WillOnce(Invoke(&controller_, &NativeWidgetNSWindowFullscreenController::
                                         OnWindowWillEnterFullscreen));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Complete the transition to fullscreen on the new display.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(true))
      .Times(1);
  controller_.OnWindowDidEnterFullscreen();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_TRUE(controller_.GetTargetFullscreenState());

  // Exit fullscreen. This will post a task to restore bounds.
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionStart(false))
      .Times(1);
  controller_.OnWindowWillExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
  task_environment_.RunUntilIdle();
  controller_.OnWindowDidExitFullscreen();
  EXPECT_TRUE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());

  // Let the run loop run, it will restore the bounds (and not crash).
  // The original bounds should still be stored, even though they were
  // previously restored, since the second fullscreen transition begun while
  // already in a transition to restore bounds after exiting fullscreen.
  EXPECT_CALL(mock_client_, FullscreenControllerSetFrame(kWindowRect, true, _))
      .WillOnce(FullscreenControllerSetFrameFake);
  EXPECT_CALL(mock_client_, FullscreenControllerTransitionComplete(false))
      .Times(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller_.IsInFullscreenTransition());
  EXPECT_FALSE(controller_.GetTargetFullscreenState());
}

}  // namespace remote_cocoa::test
