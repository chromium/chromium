// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/scroll_bar_button.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class MockButtonCallback {
 public:
  MockButtonCallback() = default;
  MockButtonCallback(const MockButtonCallback&) = delete;
  MockButtonCallback& operator=(const MockButtonCallback&) = delete;
  ~MockButtonCallback() = default;

  MOCK_METHOD(void, ButtonPressed, ());
};

class ScrollBarButtonTest : public testing::Test {
 public:
  ScrollBarButtonTest()
      : button_(std::make_unique<ScrollBarButton>(
            base::BindRepeating(&MockButtonCallback::ButtonPressed,
                                base::Unretained(&callback_)),
            ScrollBarButton::Type::kLeft,
            task_environment_.GetMockTickClock())) {
    display::Screen::SetScreenInstance(&test_screen_);
  }

  ScrollBarButtonTest(const ScrollBarButtonTest&) = delete;
  ScrollBarButtonTest& operator=(const ScrollBarButtonTest&) = delete;
  ~ScrollBarButtonTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

 protected:
  testing::StrictMock<MockButtonCallback>& callback() { return callback_; }
  Button* button() { return button_.get(); }

  void AdvanceTime(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  display::test::TestScreen test_screen_;

  testing::StrictMock<MockButtonCallback> callback_;
  const std::unique_ptr<Button> button_;
};

}  // namespace

TEST_F(ScrollBarButtonTest, Metadata) {
  test::TestViewMetadata(button());
}

TEST_F(ScrollBarButtonTest, FocusBehavior) {
  EXPECT_EQ(View::FocusBehavior::NEVER, button()->GetFocusBehavior());
}

TEST_F(ScrollBarButtonTest, CallbackFiresOnMouseDown) {
  EXPECT_CALL(callback(), ButtonPressed());

  // By default the button should notify its callback on mouse release.
  button()->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
}

TEST_F(ScrollBarButtonTest, CallbackFiresMultipleTimesMouseHeldDown) {
  EXPECT_CALL(callback(), ButtonPressed()).Times(AtLeast(2));

  // By default the button should notify its callback on mouse release.
  button()->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);
}

TEST_F(ScrollBarButtonTest, CallbackStopsFiringAfterMouseReleased) {
  EXPECT_CALL(callback(), ButtonPressed()).Times(AtLeast(2));

  // By default the button should notify its callback on mouse release.
  button()->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  testing::Mock::VerifyAndClearExpectations(&callback());

  button()->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  EXPECT_CALL(callback(), ButtonPressed()).Times(AtMost(0));
}

TEST_F(ScrollBarButtonTest, CallbackStopsFiringAfterMouseCaptureReleased) {
  EXPECT_CALL(callback(), ButtonPressed()).Times(AtLeast(2));

  // By default the button should notify its callback on mouse release.
  button()->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  testing::Mock::VerifyAndClearExpectations(&callback());

  button()->OnMouseCaptureLost();

  AdvanceTime(RepeatController::GetInitialWaitForTesting() * 10);

  EXPECT_CALL(callback(), ButtonPressed()).Times(AtMost(0));
}

}  // namespace views
