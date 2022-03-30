// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/pointer_events_handler.h"

#include <fuchsia/ui/pointer/cpp/fidl.h>
#include <gtest/gtest.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/zx/time.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/fuchsia/fakes/fake_mouse_source.h"
#include "ui/events/fuchsia/fakes/fake_touch_source.h"
#include "ui/events/fuchsia/fakes/pointer_event_utility.h"
#include "ui/events/types/event_type.h"

namespace ui {
namespace {

namespace fup = fuchsia::ui::pointer;

constexpr std::array<std::array<float, 2>, 2> kRect = {{{0, 0}, {20, 20}}};
constexpr std::array<float, 9> kIdentity = {1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr fup::TouchInteractionId kIxnOne = {.device_id = 1u,
                                             .pointer_id = 1u,
                                             .interaction_id = 2u};
constexpr uint32_t kMouseDeviceId = 123;

// Fixture to exercise the implementation for fuchsia.ui.pointer.TouchSource and
// fuchsia.ui.pointer.MouseSource.
class PointerEventsHandlerTest : public ::testing::Test {
 protected:
  PointerEventsHandlerTest() {
    touch_source_ = std::make_unique<FakeTouchSource>();
    mouse_source_ = std::make_unique<FakeMouseSource>();
    pointer_handler_ = std::make_unique<PointerEventsHandler>(
        touch_source_bindings_.AddBinding(touch_source_.get()),
        mouse_source_bindings_.AddBinding(mouse_source_.get()));
  }

  void RunLoopUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<FakeTouchSource> touch_source_;
  std::unique_ptr<FakeMouseSource> mouse_source_;
  std::unique_ptr<PointerEventsHandler> pointer_handler_;

 private:
  fidl::BindingSet<fup::TouchSource> touch_source_bindings_;
  fidl::BindingSet<fup::MouseSource> mouse_source_bindings_;
};

TEST_F(PointerEventsHandlerTest, Watch_EventCallbacksAreIndependent) {
  std::vector<std::unique_ptr<Event>> events;
  pointer_handler_->StartWatching(base::BindLambdaForTesting(
      [&events](Event* event) { events.push_back(Event::Clone(*event)); }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector<fup::TouchEvent> touch_events =
      TouchEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(touch_events));
  RunLoopUntilIdle();

  ASSERT_EQ(events.size(), 1u);
  EXPECT_TRUE(events[0]->IsTouchEvent());
  EXPECT_EQ(events[0]->AsTouchEvent()->pointer_details().pointer_type,
            EventPointerType::kTouch);

  std::vector<fup::MouseEvent> mouse_events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0}, {0, 0})
          .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(mouse_events));
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

  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111789u /* in nanoseconds */)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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

  // Fuchsia button press -> Chrome ET_MOUSE_PRESSED and EF_RIGHT_MOUSE_BUTTON
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0 /*button id*/}, {0, 0})
          .AddMouseDeviceInfo(kMouseDeviceId,
                              {2 /*first button id*/, 0 /*second button id*/,
                               1 /*third button id*/})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Keep Fuchsia button press -> Chrome ET_MOUSE_DRAGGED and
  // EF_RIGHT_MOUSE_BUTTON
  events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0 /*button id*/}, {0, 0})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_DRAGGED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Release Fuchsia button -> Chrome ET_MOUSE_RELEASED
  events = MouseEventBuilder()
               .AddTime(1111789u)
               .AddSample(kMouseDeviceId, {10.f, 10.f}, {}, {0, 0})
               .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Release Fuchsia button -> Chrome ET_MOUSE_MOVED
  events = MouseEventBuilder()
               .AddTime(1111789u)
               .AddSample(kMouseDeviceId, {10.f, 10.f}, {}, {0, 0})
               .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_MOVED);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  mouse_events.clear();
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeMouseEventFlagsAreSynthesized) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia button press -> Chrome ET_MOUSE_PRESSED and EF_RIGHT_MOUSE_BUTTON
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0}, {0, 0})
          .AddMouseDeviceInfo(kMouseDeviceId, {2, 0, 1})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();

  // Switch Fuchsia button press -> Chrome ET_MOUSE_DRAGGED and
  // EF_LEFT_MOUSE_BUTTON
  events = MouseEventBuilder()
               .AddTime(1111789u)
               .AddSample(kMouseDeviceId, {10.f, 10.f}, {2}, {0, 0})
               .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_RELEASED);
  EXPECT_EQ(mouse_events[1].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();
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
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0, 1}, {0, 0})
          .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0].flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1].type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[1].flags(), EF_RIGHT_MOUSE_BUTTON);
  mouse_events.clear();
}

TEST_F(PointerEventsHandlerTest, MouseMultiButtonDrag) {
  std::vector<MouseEvent> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        mouse_events.push_back(*event->AsMouseEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector<fup::MouseEvent> events;
  // Press left and right button.
  events.push_back(MouseEventBuilder()
                       .AddTime(1111789u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {10.f, 10.f}, {0, 1}, {0, 0})
                       .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
                       .Build());
  // drag with left, right button pressing.
  events.push_back(MouseEventBuilder()
                       .AddTime(1111790u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {11.f, 10.f}, {0, 1}, {0, 0})
                       .Build());
  // right button up.
  events.push_back(MouseEventBuilder()
                       .AddTime(1111791u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {11.f, 10.f}, {0}, {0, 0})
                       .Build());
  // drag with left button pressing.
  events.push_back(MouseEventBuilder()
                       .AddTime(1111792u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {11.f, 11.f}, {0}, {0, 0})
                       .Build());
  // left button up.
  events.push_back(MouseEventBuilder()
                       .AddTime(11117913u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {11.f, 11.f}, {}, {0, 0})
                       .Build());
  // mouse move.
  events.push_back(MouseEventBuilder()
                       .AddTime(1111794u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {12.f, 11.f}, {}, {0, 0})
                       .Build());

  mouse_source_->ScheduleCallback(std::move(events));
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
  mouse_events.clear();
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
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111789u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {}, {0, 1})
          .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
          .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 120);
  mouse_events.clear();

  // receive a horizontal scroll
  events = MouseEventBuilder()
               .AddTime(1111789u)
               .AddViewParameters(kRect, kRect, kIdentity)
               .AddSample(kMouseDeviceId, {10.f, 10.f}, {}, {1, 0})
               .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
               .BuildAsVector();
  mouse_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 1u);
  EXPECT_EQ(mouse_events[0].type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[0].flags(), EF_NONE);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->x_offset(), 120);
  EXPECT_EQ(mouse_events[0].AsMouseWheelEvent()->y_offset(), 0);
  mouse_events.clear();
}

TEST_F(PointerEventsHandlerTest, MouseWheelEventWithButtonPressed) {
  std::vector<std::unique_ptr<Event>> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_TRUE(event->IsMouseEvent());
        if (event->IsMouseWheelEvent()) {
          auto e = Event::Clone(*event->AsMouseWheelEvent());
          mouse_events.push_back(std::move(e));
        } else if (event->IsMouseEvent()) {
          auto e = Event::Clone(*event->AsMouseEvent());
          mouse_events.push_back(std::move(e));
        } else {
          NOTREACHED();
        }
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // left button down
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0}, {0, 0})
          .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
          .BuildAsVector();

  // receive a vertical scroll with pressed button
  events.push_back(MouseEventBuilder()
                       .AddTime(1111789u)
                       .AddViewParameters(kRect, kRect, kIdentity)
                       .AddSample(kMouseDeviceId, {10.f, 10.f}, {0}, {0, 1})
                       .Build());
  mouse_source_->ScheduleCallback(std::move(events));

  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0]->type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[1]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->y_offset(), 120);

  mouse_events.clear();
}

TEST_F(PointerEventsHandlerTest, MouseWheelEventWithButtonDownBundled) {
  std::vector<std::unique_ptr<Event>> mouse_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&mouse_events](Event* event) {
        ASSERT_TRUE(event->IsMouseEvent());
        if (event->IsMouseWheelEvent()) {
          auto e = Event::Clone(*event->AsMouseWheelEvent());
          mouse_events.push_back(std::move(e));
        } else if (event->IsMouseEvent()) {
          auto e = Event::Clone(*event->AsMouseEvent());
          mouse_events.push_back(std::move(e));
        } else {
          NOTREACHED();
        }
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // left button down and a vertical scroll bundled.
  std::vector<fup::MouseEvent> events =
      MouseEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kMouseDeviceId, {10.f, 10.f}, {0}, {0, 1})
          .AddMouseDeviceInfo(kMouseDeviceId, {0, 1, 2})
          .BuildAsVector();

  mouse_source_->ScheduleCallback(std::move(events));

  RunLoopUntilIdle();

  ASSERT_EQ(mouse_events.size(), 2u);
  EXPECT_EQ(mouse_events[0]->type(), ET_MOUSE_PRESSED);
  EXPECT_EQ(mouse_events[0]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->type(), ET_MOUSEWHEEL);
  EXPECT_EQ(mouse_events[1]->flags(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->x_offset(), 0);
  EXPECT_EQ(mouse_events[1]->AsMouseWheelEvent()->y_offset(), 120);

  mouse_events.clear();
}

TEST_F(PointerEventsHandlerTest, Phase_ChromeTouchEventTypesAreSynthesized) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD -> Chrome ET_TOUCH_PRESSED
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CHANGE -> Chrome ET_TOUCH_MOVED
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_MOVED);
  touch_events.clear();

  // Fuchsia REMOVE -> Chrome ET_TOUCH_RELEASED
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddSample(kIxnOne, fup::EventPhase::REMOVE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_RELEASED);
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Phase_FuchsiaCancelBecomesChromeCancel) {
  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  // Fuchsia ADD -> Chrome ET_TOUCH_PRESSED
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CANCEL -> Chrome CANCEL
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CANCEL, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_CANCELLED);
  touch_events.clear();
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(2222000u)
          .AddViewParameters(/*view*/ {{{0, 0}, {20, 20}}},
                             /*viewport*/ {{{0, 0}, {20, 20}}},
                             /*matrix*/ {1, 0, 0, 0, 1, 0, 0, 0, 1})
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
  touch_events.clear();

  // Fuchsia CHANGE event, with a view parameter that translates the viewport by
  // (10, 10) within the view. Then the minimal point in the viewport (its
  // origin) should map to the center of the view, (10.f, 10.f).
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddViewParameters(/*view*/ {{{0, 0}, {20, 20}}},
                                  /*viewport*/ {{{0, 0}, {20, 20}}},
                                  /*matrix*/ {1, 0, 0, 0, 1, 0, 10, 10, 1})
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {0.f, 0.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
  touch_events.clear();

  // Fuchsia CHANGE event, with a view parameter that scales the viewport by
  // (0.5, 0.5) within the view. Then the maximal point in the viewport should
  // map to the center of the view, (10.f, 10.f).
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddViewParameters(/*view*/ {{{0, 0}, {20, 20}}},
                                  /*viewport*/ {{{0, 0}, {20, 20}}},
                                  /*matrix*/ {0.5f, 0, 0, 0, 0.5f, 0, 0, 0, 1})
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {20.f, 20.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].location_f().x(), 10.f);
  EXPECT_EQ(touch_events[0].location_f().y(), 10.f);
  touch_events.clear();
}

TEST_F(PointerEventsHandlerTest, Coordinates_PressedEventClampedToView) {
  const float kSmallDiscrepancy = -0.00003f;

  std::vector<TouchEvent> touch_events;
  pointer_handler_->StartWatching(
      base::BindLambdaForTesting([&touch_events](Event* event) {
        touch_events.push_back(*event->AsTouchEvent());
      }));
  RunLoopUntilIdle();  // Server gets watch call.

  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, kSmallDiscrepancy})
          .AddResult({.interaction = kIxnOne,
                      .status = fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  const auto responses = touch_source_->UploadedResponses();
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

  // Fuchsia view parameter only. Empty response.
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .BuildAsVector();

  // Fuchsia ptr 1 ADD sample. Yes response.
  fup::TouchEvent e1 =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample({.device_id = 0u, .pointer_id = 1u, .interaction_id = 3u},
                     fup::EventPhase::ADD, {10.f, 10.f})
          .Build();
  events.emplace_back(std::move(e1));

  // Fuchsia ptr 2 ADD sample. Yes response.
  fup::TouchEvent e2 =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddSample({.device_id = 0u, .pointer_id = 2u, .interaction_id = 3u},
                     fup::EventPhase::ADD, {5.f, 5.f})
          .Build();
  events.emplace_back(std::move(e2));

  // Fuchsia ptr 3 ADD sample. Yes response.
  fup::TouchEvent e3 =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddSample({.device_id = 0u, .pointer_id = 3u, .interaction_id = 3u},
                     fup::EventPhase::ADD, {1.f, 1.f})
          .Build();
  events.emplace_back(std::move(e3));
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  const auto responses = touch_source_->UploadedResponses();
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership granted. Buffered pointers released.
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddResult({kIxnOne, fup::TouchInteractionStatus::GRANTED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 2u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[1].type(), ET_TOUCH_MOVED);
  touch_events.clear();

  // Fuchsia CHANGE, grant result - release immediately.
  events = TouchEventBuilder()
               .AddTime(/* in nanoseconds */ 4444000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, with grant result - release buffered events.
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .AddResult({kIxnOne, fup::TouchInteractionStatus::GRANTED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .AddResult({kIxnOne, fup::TouchInteractionStatus::GRANTED})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  touch_events.clear();

  // Fuchsia CHANGE, after grant result - release immediately.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership denied. Buffered pointers deleted.
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddResult({kIxnOne, fup::TouchInteractionStatus::DENIED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia CHANGE, no grant result - buffer it.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddSample(kIxnOne, fup::EventPhase::CHANGE, {10.f, 10.f})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Fuchsia result: ownership denied. Buffered pointers deleted.
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddSample(kIxnOne, fup::EventPhase::CANCEL, {10.f, 10.f})
               .AddResult({kIxnOne, fup::TouchInteractionStatus::DENIED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
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
  std::vector<fup::TouchEvent> events =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddViewParameters(kRect, kRect, kIdentity)
          .AddSample(kIxnOne, fup::EventPhase::ADD, {10.f, 10.f})
          .BuildAsVector();
  fup::TouchEvent ptr2 =
      TouchEventBuilder()
          .AddTime(1111000u)
          .AddSample(kIxnTwo, fup::EventPhase::ADD, {15.f, 15.f})
          .Build();
  events.emplace_back(std::move(ptr2));
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 0u);
  touch_events.clear();

  // Server grants win to pointer 2.
  events = TouchEventBuilder()
               .AddTime(2222000u)
               .AddResult({kIxnTwo, fup::TouchInteractionStatus::GRANTED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[0].pointer_details().id, 2);
  touch_events.clear();

  // Server grants win to pointer 1.
  events = TouchEventBuilder()
               .AddTime(3333000u)
               .AddResult({kIxnOne, fup::TouchInteractionStatus::GRANTED})
               .BuildAsVector();
  touch_source_->ScheduleCallback(std::move(events));
  RunLoopUntilIdle();

  ASSERT_EQ(touch_events.size(), 1u);
  EXPECT_EQ(touch_events[0].type(), ET_TOUCH_PRESSED);
  EXPECT_EQ(touch_events[0].pointer_details().id, 1);
  touch_events.clear();
}

}  // namespace
}  // namespace ui
