// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/pointer_events_handler.h"

#include <fuchsia/ui/pointer/cpp/fidl.h>
#include <gtest/gtest.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/ui/scenic/cpp/testing/fake_mouse_source.h>
#include <lib/ui/scenic/cpp/testing/fake_touch_source.h>
#include <lib/zx/time.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/fuchsia/util/pointer_event_utility.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {
namespace {

namespace fup = fuchsia::ui::pointer;

// Builds a vector of move-only values.
template <typename T, class... Args>
std::vector<T> MakeVector(Args&&... args) {
  std::vector<T> output;
  output.reserve(sizeof...(Args));
  ((output.emplace_back(std::forward<Args>(args))), ...);
  return output;
}

// Fixture to exercise the implementation for fuchsia.ui.pointer.TouchSource and
// fuchsia.ui.pointer.MouseSource.
class PointerEventsHandlerTest : public ::testing::Test {
 protected:
  PointerEventsHandlerTest()
      : fake_touch_source_binding_(&fake_touch_source_),
        fake_mouse_source_binding_(&fake_mouse_source_) {
    pointer_handler_ = std::make_unique<PointerEventsHandler>(
        fake_touch_source_binding_.NewBinding(),
        fake_mouse_source_binding_.NewBinding());
  }

  ~PointerEventsHandlerTest() override { MouseEvent::ResetLastClickForTest(); }

  void RunLoopUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  scenic::FakeTouchSource fake_touch_source_;
  scenic::FakeMouseSource fake_mouse_source_;
  std::unique_ptr<PointerEventsHandler> pointer_handler_;

 private:
  fidl::Binding<fup::TouchSource> fake_touch_source_binding_;
  fidl::Binding<fup::MouseSource> fake_mouse_source_binding_;
};

TEST_F(PointerEventsHandlerTest, Watch_EventCallbacksAreIndependent) {
  std::vector<std::unique_ptr<Event>> events;
  pointer_handler_->StartWatching(base::BindLambdaForTesting(
      [&events](Event* event) { events.push_back(event->Clone()); }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector touch_events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(touch_events));
  RunLoopUntilIdle();

  ASSERT_EQ(events.size(), 1u);
  EXPECT_TRUE(events[0]->IsTouchEvent());
  EXPECT_EQ(events[0]->AsTouchEvent()->pointer_details().pointer_type,
            EventPointerType::kTouch);

  std::vector mouse_events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(mouse_events));
  RunLoopUntilIdle();

  ASSERT_EQ(events.size(), 2u);
  EXPECT_TRUE(events[1]->IsMouseEvent());
  EXPECT_EQ(events[1]->AsMouseEvent()->pointer_details().pointer_type,
            EventPointerType::kMouse);
}

TEST_F(PointerEventsHandlerTest, Data_FuchsiaTimeVersusChromeTime) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{1111783u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(zx::nsec(touch_events[0].time_stamp().ToZxTime()).to_usecs(),
            /* in microseconds */ 1111u);
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeMouseEventTypesAreSynthesized) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia button press -> Chrome ET_MOUSE_PRESSED and
  // EF_RIGHT_MOUSE_BUTTON
  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).SetButtons({2, 0, 1}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Keep Fuchsia button press -> Chrome ET_MOUSE_DRAGGED and
  // EF_RIGHT_MOUSE_BUTTON
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetPressedButtons({0})
                                           .WithoutViewParameters()
                                           .WithoutDeviceInfo()
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_DRAGGED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Release Fuchsia button -> Chrome ET_MOUSE_RELEASED
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetPressedButtons({})
                                           .WithoutViewParameters()
                                           .WithoutDeviceInfo()
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Release Fuchsia button -> Chrome ET_MOUSE_MOVED
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetPressedButtons({})
                                           .WithoutViewParameters()
                                           .WithoutDeviceInfo()
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_MOVED);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeMouseEventFlagsAreSynthesized) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia button press -> Chrome ET_MOUSE_PRESSED and EF_RIGHT_MOUSE_BUTTON
  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).SetButtons({2, 0, 1}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Switch Fuchsia button press -> Chrome ET_MOUSE_DRAGGED and
  // EF_LEFT_MOUSE_BUTTON
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetPressedButtons({2})
                                           .WithoutViewParameters()
                                           .WithoutDeviceInfo()
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[1].flags(), EF_RIGHT_MOUSE_BUTTON);
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeMouseEventFlagCombo) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia button press -> Chrome ET_MOUSE_PRESSED on EF_LEFT_MOUSE_BUTTON
  // and EF_RIGHT_MOUSE_BUTTON
  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0, 1}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[1].flags(), EF_RIGHT_MOUSE_BUTTON);
}

TEST_F(PointerEventsHandlerTest, ChromeMouseEventDoubleClick) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).IncrementTime().Build(),
      MouseEventBuilder().SetPressedButtons({}).IncrementTime().Build(),
      MouseEventBuilder().SetPressedButtons({0}).IncrementTime().Build(),
      MouseEventBuilder().SetPressedButtons({}).IncrementTime().Build());

  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 4u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[1].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[2].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[2].flags(), EF_LEFT_MOUSE_BUTTON | EF_IS_DOUBLE_CLICK);
  EXPECT_EQ(mouse_events[3].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[3].flags(), EF_LEFT_MOUSE_BUTTON | EF_IS_DOUBLE_CLICK);
}

TEST_F(PointerEventsHandlerTest, MouseMultiButtonDrag) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::MouseEvent>(
      // Press left and right button.
      MouseEventBuilder()
          .SetPosition({10.f, 10.f})
          .SetPressedButtons({0, 1})
          .IncrementTime()
          .Build(),
      // drag with left, right button pressing.
      MouseEventBuilder()
          .SetPosition({11.f, 10.f})
          .SetPressedButtons({0, 1})
          .IncrementTime()
          .Build(),
      // right button up.
      MouseEventBuilder()
          .SetPosition({11.f, 10.f})
          .SetPressedButtons({0})
          .IncrementTime()
          .Build(),
      // drag with left button pressing.
      MouseEventBuilder()
          .SetPosition({11.f, 11.f})
          .SetPressedButtons({0})
          .IncrementTime()
          .Build(),
      // left button up.
      MouseEventBuilder().SetPosition({11.f, 11.f}).IncrementTime().Build(),
      // mouse move.
      MouseEventBuilder().SetPosition({12.f, 11.f}).IncrementTime().Build());

  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 7u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[1].flags(), EF_RIGHT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[2].type(), ET_MOUSE_DRAGGED);
  EXPECT_EQ(mouse_events[2].flags(),
            EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[3].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[3].flags(), EF_RIGHT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[4].type(), ET_MOUSE_DRAGGED);
  EXPECT_EQ(mouse_events[4].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[5].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[5].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[6].type(), ET_MOUSE_MOVED);
  EXPECT_EQ(mouse_events[6].flags(), 0);
}

TEST_F(PointerEventsHandlerTest, MouseWheelEvent) {
  std::vector<MouseWheelEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_EQ(event->type(), ET_MOUSEWHEEL);
        mouse_events.push_back(*event->AsMouseWheelEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // receive a vertical scroll
  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetScroll({0, 1}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 120);
  mouse_events.clear();

  // receive a horizontal scroll
  events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetScroll({1, 0}).Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 120);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 0);
}

TEST_F(PointerEventsHandlerTest, MouseWheelEventDeltaInPhysicalPixel) {
  std::vector<MouseWheelEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_EQ(event->type(), ET_MOUSEWHEEL);
        mouse_events.push_back(*event->AsMouseWheelEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // receive a vertical scroll
  std::vector events =
      MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                      .SetScroll({0, 1})
                                      .SetScrollInPhysicalPixel({0, 100})
                                      .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 100);
  mouse_events.clear();

  // receive a horizontal scroll
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetScroll({1, 0})
                                           .SetScrollInPhysicalPixel({100, 0})
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 100);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 0);
}

TEST_F(PointerEventsHandlerTest, ScrollEventDeltaInPhysicalPixel) {
  std::vector<ScrollEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_EQ(event->type(), ET_SCROLL);
        mouse_events.push_back(*event->AsScrollEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // receive a vertical scroll
  std::vector events =
      MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                      .SetScroll({0, 1})
                                      .SetScrollInPhysicalPixel({0, 100})
                                      .SetIsPrecisionScroll(true)
                                      .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_SCROLL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->y_offset(), 100);
  mouse_events.clear();

  // receive a horizontal scroll
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetScroll({1, 0})
                                           .SetScrollInPhysicalPixel({100, 0})
                                           .SetIsPrecisionScroll(true)
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_SCROLL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->x_offset(), 100);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->y_offset(), 0);
}

TEST_F(PointerEventsHandlerTest, ScrollEventDeltaInPhysicalPixelNoTickDelta) {
  std::vector<ScrollEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_EQ(event->type(), ET_SCROLL);
        mouse_events.push_back(*event->AsScrollEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // receive a vertical scroll
  std::vector events =
      MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                      .SetScrollInPhysicalPixel({0, 100})
                                      .SetIsPrecisionScroll(true)
                                      .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_SCROLL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->y_offset(), 100);
  mouse_events.clear();

  // receive a horizontal scroll
  events = MakeVector<fup::MouseEvent>(MouseEventBuilder()
                                           .SetScrollInPhysicalPixel({100, 0})
                                           .SetIsPrecisionScroll(true)
                                           .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_SCROLL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->x_offset(), 100);
  EXPECT_EQ(mouse_events[0].AsScrollEvent()->y_offset(), 0);
}

TEST_F(PointerEventsHandlerTest, MouseWheelEventWithButtonPressed) {
  std::vector<std::unique_ptr<Event>> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_TRUE(event->IsMouseEvent());
        if (event->IsMouseWheelEvent()) {
          auto e = event->AsMouseWheelEvent()->Clone();
          mouse_events.push_back(std::move(e));
        } else if (event->IsMouseEvent()) {
          auto e = event->AsMouseEvent()->Clone();
          mouse_events.push_back(std::move(e));
        } else {
          NOTREACHED();
        }
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).IncrementTime().Build(),
      // receive a vertical scroll with pressed button
      MouseEventBuilder()
          .SetPressedButtons({0})
          .SetScroll({0, 1})
          .IncrementTime()
          .Build());
  fake_mouse_source_.ScheduleCallback(std::move(events));

  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0]->type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[1]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->y_offset(), 120);
}

TEST_F(PointerEventsHandlerTest, MouseWheelEventWithButtonDownBundled) {
  std::vector<std::unique_ptr<Event>> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_TRUE(event->IsMouseEvent());
        if (event->IsMouseWheelEvent()) {
          auto e = event->AsMouseWheelEvent()->Clone();
          mouse_events.push_back(std::move(e));
        } else if (event->IsMouseEvent()) {
          auto e = event->AsMouseEvent()->Clone();
          mouse_events.push_back(std::move(e));
        } else {
          NOTREACHED();
        }
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // left button down and a vertical scroll bundled.
  std::vector events = MakeVector<fup::MouseEvent>(
      MouseEventBuilder().SetPressedButtons({0}).SetScroll({0, 1}).Build());

  fake_mouse_source_.ScheduleCallback(std::move(events));

  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0]->type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[1]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->y_offset(), 120);
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeTouchEventTypesAreSynthesized) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD -> Chrome ET_TOUCH_PRESSED
  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{1111000u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CHANGE -> Chrome ET_TOUCH_MOVED
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_MOVED);
  touch_events.clear();

  // Fuchsia REMOVE -> Chrome ET_TOUCH_RELEASED
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{3333000u})
                                           .SetPhase(fup::EventPhase::REMOVE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_RELEASED);
}

TEST_F(PointerEventsHandlerTest, Phase_FuchsiaCancelBecomesChromeCancel) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD -> Chrome ET_TOUCH_PRESSED
  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{1111000u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CANCEL -> Chrome CANCEL
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CANCEL)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_CANCELLED);
}

TEST_F(PointerEventsHandlerTest, Coordinates_CorrectMapping) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD event, with a view parameter that maps the viewport identically
  // to the view. Then the center point of the viewport should map to the center
  // of the view, (10.f, 10.f).
  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{2222000u})
          .SetView(gfx::RectF(0, 0, 20, 20))
          .SetViewport(gfx::RectF(0, 0, 20, 20))
          .SetTransform({1, 0, 0, 0, 1, 0, 0, 0, 1})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
  touch_events.clear();

  // Fuchsia CHANGE event, with a view parameter that translates the viewport by
  // (10, 10) within the view. Then the minimal point in the viewport (its
  // origin) should map to the center of the view, (10.f, 10.f).
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{2222000u})
          .SetView(gfx::RectF(0, 0, 20, 20))
          .SetViewport(gfx::RectF(0, 0, 20, 20))
          .SetTransform({1, 0, 0, 0, 1, 0, 10, 10, 1})
          .SetPhase(fup::EventPhase::CHANGE)
          .SetPosition({0.f, 0.f})
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
  touch_events.clear();

  // Fuchsia CHANGE event, with a view parameter that scales the viewport by
  // (0.5, 0.5) within the view. Then the maximal point in the viewport should
  // map to the center of the view, (10.f, 10.f).
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{2222000u})
          .SetView(gfx::RectF(0, 0, 20, 20))
          .SetViewport(gfx::RectF(0, 0, 20, 20))
          .SetTransform({0.5f, 0, 0, 0, 0.5f, 0, 0, 0, 1})
          .SetPhase(fup::EventPhase::CHANGE)
          .SetPosition({20.f, 20.f})
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
}

TEST_F(PointerEventsHandlerTest, Coordinates_PressedEventClampedToView) {
  const float kSmallDiscrepancy = -0.00003f;

  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetPosition({10.f, kSmallDiscrepancy})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), kSmallDiscrepancy);
}

TEST_F(PointerEventsHandlerTest, Protocol_FirstResponseIsEmpty) {
  bool called = false;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&called](Event*) { called = true; }));
  RunLoopUntilIdle();  // Server gets Watch call.

  EXPECT_FALSE(called);  // No events yet received to forward to client.
  // Server sees an initial "response" from client, which is empty, by contract.
  const auto responses = fake_touch_source_.UploadedResponses();
  ASSERT_TRUE(responses.has_value());
  ASSERT_EQ(responses->size(), 0u);
}

TEST_F(PointerEventsHandlerTest, Protocol_ResponseMatchesEarlierEvents) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector events = MakeVector<fup::TouchEvent>(
      // Fuchsia view parameter only. Empty response.
      TouchEventBuilder().WithoutSample().Build(),

      // Fuchsia ptr 1 ADD sample. Yes response.
      TouchEventBuilder()
          .SetId({.device_id = 0u, .pointer_id = 1u, .interaction_id = 3u})
          .SetPosition({10.f, 10.f})
          .Build(),

      // Fuchsia ptr 2 ADD sample. Yes response.
      TouchEventBuilder()
          .SetId({.device_id = 0u, .pointer_id = 2u, .interaction_id = 3u})
          .SetPosition({5.f, 5.f})
          .Build(),

      // Fuchsia ptr 3 ADD sample. Yes response.
      TouchEventBuilder()
          .SetId({.device_id = 0u, .pointer_id = 3u, .interaction_id = 3u})
          .SetPosition({1.f, 1.f})
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  const auto responses = fake_touch_source_.UploadedResponses();
  ASSERT_TRUE(responses.has_value());
  ASSERT_EQ(responses.value().size(), 4u);
  // Event 0 did not carry a sample, so no response.
  EXPECT_FALSE(responses.value()[0].has_response_type());
  // Events 1-3 had a sample, must have a response.
  EXPECT_TRUE(responses.value()[1].has_response_type());
  EXPECT_EQ(responses.value()[1].response_type(), fup::TouchResponseType::YES);
  EXPECT_TRUE(responses.value()[2].has_response_type());
  EXPECT_EQ(responses.value()[2].response_type(), fup::TouchResponseType::YES);
  EXPECT_TRUE(responses.value()[3].has_response_type());
  EXPECT_EQ(responses.value()[3].response_type(), fup::TouchResponseType::YES);
}

TEST_F(PointerEventsHandlerTest, Protocol_LateGrant) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD, no grant result - buffer it.
  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder().SetTime(zx::time{1111000u}).Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership granted. Buffered pointers released.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{3333000u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .WithoutSample()
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 2u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[1].type(), ET_TOUCH_MOVED);
  touch_events.clear();

  // Fuchsia CHANGE, grant result - release immediately.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{4444000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_MOVED);
  EXPECT_EQ(zx::nsec(touch_events[0].time_stamp().ToZxTime()).to_usecs(),
            /* in microseconds */ 4444u);
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Protocol_LateGrantCombo) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD, no grant result - buffer it.
  std::vector events =
      MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                      .SetTime(zx::time{1111000u})
                                      .SetPhase(fup::EventPhase::ADD)
                                      .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, with grant result - release buffered events.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{3333000u})
          .SetPhase(fup::EventPhase::CHANGE)
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 3u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(zx::nsec(touch_events[0].time_stamp().ToZxTime()).to_usecs(),
            /* in microseconds */ 1111u);
  EXPECT_EQ(touch_events[1].type(), ET_TOUCH_MOVED);
  EXPECT_EQ(zx::nsec(touch_events[1].time_stamp().ToZxTime()).to_usecs(),
            /* in microseconds */ 2222u);
  EXPECT_EQ(touch_events[2].type(), ET_TOUCH_MOVED);
  EXPECT_EQ(zx::nsec(touch_events[2].time_stamp().ToZxTime()).to_usecs(),
            /* in microseconds */ 3333u);
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Protocol_EarlyGrant) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD, with grant result - release immediately.
  std::vector events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{1111000u})
          .SetPhase(fup::EventPhase::ADD)
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CHANGE, after grant result - release immediately.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_MOVED);
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Protocol_LateDeny) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD, no grant result - buffer it.
  std::vector events =
      MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                      .SetTime(zx::time{1111000u})
                                      .SetPhase(fup::EventPhase::ADD)
                                      .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership denied. Buffered pointers deleted.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{3333000u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::DENIED)
          .WithoutSample()
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);  // Do not release to client!
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Protocol_LateDenyCombo) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD, no grant result - buffer it.
  std::vector events =
      MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                      .SetTime(zx::time{1111000u})
                                      .SetPhase(fup::EventPhase::ADD)
                                      .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                           .SetTime(zx::time{2222000u})
                                           .SetPhase(fup::EventPhase::CHANGE)
                                           .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership denied. Buffered pointers deleted.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{3333000u})
          .SetPhase(fup::EventPhase::CANCEL)
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::DENIED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);  // Do not release to client!
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Protocol_PointersAreIndependent) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  constexpr fup::TouchInteractionId kIxnTwo = {
      .device_id = 1u, .pointer_id = 2u, .interaction_id = 2u};

  // Fuchsia ptr1 ADD and ptr2 ADD, no grant result for either - buffer them.
  std::vector events =
      MakeVector<fup::TouchEvent>(TouchEventBuilder()
                                      .SetTime(zx::time{1111000u})
                                      .SetPhase(fup::EventPhase::ADD)
                                      .Build(),
                                  TouchEventBuilder()
                                      .SetTime(zx::time{1111000u})
                                      .SetId(kIxnTwo)
                                      .SetPhase(fup::EventPhase::ADD)
                                      .SetPosition({15.f, 15.f})
                                      .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Server grants win to pointer 2.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{2222000u})
          .SetId(kIxnTwo)
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .WithoutSample()
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[0].pointer_details().id, 2);
  touch_events.clear();

  // Server grants win to pointer 1.
  events = MakeVector<fup::TouchEvent>(
      TouchEventBuilder()
          .SetTime(zx::time{3333000u})
          .SetTouchInteractionStatus(fup::TouchInteractionStatus::GRANTED)
          .WithoutSample()
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[0].pointer_details().id, 1);
  touch_events.clear();
}

}  // namespace
}  // namespace ui
