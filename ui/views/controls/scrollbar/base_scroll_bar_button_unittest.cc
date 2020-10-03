// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/repeat_controller.h"
#include "ui/views/test/view_metadata_test_utils.h"

namespace views {

namespace {

using testing::_;
using testing::AtLeast;
using testing::AtMost;

class MockButtonListener : public ButtonListener {
 public:
  MockButtonListener() = default;
  MockButtonListener(const MockButtonListener&) = delete;
  MockButtonListener& operator=(const MockButtonListener&) = delete;
  ~MockButtonListener() override = default;

  // ButtonListener:
  MOCK_METHOD(void,
              ButtonPressed,
              (Button * sender, const ui::Event& event),
              (override));
};

class BaseScrollBarButtonTest : public testing::Test {
 public:
  BaseScrollBarButtonTest()
      : button_(std::make_unique<BaseScrollBarButton>(
            &listener_,
            task_environment_.GetMockTickClock())) {}

  ~BaseScrollBarButtonTest() override = default;

 protected:
  testing::StrictMock<MockButtonListener>& listener() { return listener_; }
  Button* button() { return button_.get(); }

  void AdvanceTime(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  display::test::TestScreen test_screen_;
  display::test::ScopedScreenOverride screen_override{&test_screen_};

  testing::StrictMock<MockButtonListener> listener_;
  const std::unique_ptr<Button> button_;
};

}  // namespace

TEST_F(BaseScrollBarButtonTest, Metadata) {
  test::TestViewMetadata(button());
}

TEST_F(BaseScrollBarButtonTest, FocusBehavior) {
  EXPECT_EQ(View::FocusBehavior::NEVER, button()->GetFocusBehavior());
}

TEST_F(BaseScrollBarButtonTest, CallbackFiresOnMouseDown) {
  EXPECT_CALL(listener(), ButtonPressed(_, _));

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
}

TEST_F(BaseScrollBarButtonTest, CallbackFilesMultipleTimesMouseHeldDown) {
  EXPECT_CALL(listener(), ButtonPressed(_, _)).Times(AtLeast(2));

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);
}

TEST_F(BaseScrollBarButtonTest, CallbackStopsFiringAfterMouseReleased) {
  EXPECT_CALL(listener(), ButtonPressed(_, _)).Times(AtLeast(2));

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  testing::Mock::VerifyAndClearExpectations(&listener());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  EXPECT_CALL(listener(), ButtonPressed(_, _)).Times(AtMost(0));
}

TEST_F(BaseScrollBarButtonTest, CallbackStopsFiringAfterMouseCaptureReleased) {
  EXPECT_CALL(listener(), ButtonPressed(_, _)).Times(AtLeast(2));

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  testing::Mock::VerifyAndClearExpectations(&listener());

  button()->OnMouseCaptureLost();

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  EXPECT_CALL(listener(), ButtonPressed(_, _)).Times(AtMost(0));
}

}  // namespace views
