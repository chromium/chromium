// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_injector_evdev.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/testing/fake_cursor_delegate_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace ui {

using testing::AllOf;
using testing::InSequence;
using testing::Property;

class EventObserver {
 public:
  void EventDispatchCallback(Event* event) {
    DispatchEventFromNativeUiEvent(
        event, base::BindOnce(&EventObserver::OnEvent, base::Unretained(this)));
  }

  void OnEvent(Event* event) {
    if (event->IsMouseEvent()) {
      if (event->type() == EventType::kMousewheel) {
        OnMouseWheelEvent(event->AsMouseWheelEvent());
      } else {
        OnMouseEvent(event->AsMouseEvent());
      }
    }
  }

  // Mock functions for intercepting mouse events.
  MOCK_METHOD1(OnMouseEvent, void(MouseEvent* event));
  MOCK_METHOD1(OnMouseWheelEvent, void(MouseWheelEvent* event));
};

MATCHER_P4(MatchesMouseEvent, type, button, x, y, "") {
  if (arg->type() != type) {
    *result_listener << "Expected type: " << base::to_underlying(type)
                     << " actual: " << base::to_underlying(arg->type()) << " ("
                     << arg->GetName() << ")";
    return false;
  }
  if (button == EF_LEFT_MOUSE_BUTTON && !arg->IsLeftMouseButton()) {
    *result_listener << "Expected the left button flag is set.";
    return false;
  }
  if (button == EF_RIGHT_MOUSE_BUTTON && !arg->IsRightMouseButton()) {
    *result_listener << "Expected the right button flag is set.";
    return false;
  }
  if (button == EF_MIDDLE_MOUSE_BUTTON && !arg->IsMiddleMouseButton()) {
    *result_listener << "Expected the middle button flag is set.";
    return false;
  }
  if (arg->x() != x || arg->y() != y) {
    *result_listener << "Expected location: (" << x << ", " << y
                     << ") actual: (" << arg->x() << ", " << arg->y() << ")";
    return false;
  }
  return true;
}

class InputInjectorEvdevTest : public testing::Test {
 public:
  InputInjectorEvdevTest();

  InputInjectorEvdevTest(const InputInjectorEvdevTest&) = delete;
  InputInjectorEvdevTest& operator=(const InputInjectorEvdevTest&) = delete;

 protected:
  void SimulateMouseClick(int x, int y, EventFlags button, int count);
  void ExpectClick(int x, int y, int button, int count);

  EventObserver event_observer_;
  const EventDispatchCallback dispatch_callback_;
  FakeCursorDelegateEvdev cursor_;

  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<StubKeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<EventFactoryEvdev> event_factory_;

  InputInjectorEvdev injector_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
};

InputInjectorEvdevTest::InputInjectorEvdevTest()
    : dispatch_callback_(
          base::BindRepeating(&EventObserver::EventDispatchCallback,
                              base::Unretained(&event_observer_))),
      device_manager_(CreateDeviceManagerForTest()),
      keyboard_layout_engine_(std::make_unique<ui::StubKeyboardLayoutEngine>()),
      event_factory_(
          CreateEventFactoryEvdevForTest(&cursor_,
                                         device_manager_.get(),
                                         keyboard_layout_engine_.get(),
                                         dispatch_callback_)),
      injector_(CreateDeviceEventDispatcherEvdevForTest(event_factory_.get()),
                &cursor_) {}

void InputInjectorEvdevTest::SimulateMouseClick(int x,
                                                int y,
                                                EventFlags button,
                                                int count) {
  injector_.MoveCursorTo(gfx::PointF(x, y));
  for (int i = 0; i < count; i++) {
    injector_.InjectMouseButton(button, true);
    injector_.InjectMouseButton(button, false);
  }
}

void InputInjectorEvdevTest::ExpectClick(int x, int y, int button, int count) {
  InSequence dummy;
  EXPECT_CALL(event_observer_, OnMouseEvent(MatchesMouseEvent(
                                   EventType::kMouseMoved, EF_NONE, x, y)));

  for (int i = 0; i < count; i++) {
    EXPECT_CALL(event_observer_, OnMouseEvent(MatchesMouseEvent(
                                     EventType::kMousePressed, button, x, y)));
    EXPECT_CALL(event_observer_, OnMouseEvent(MatchesMouseEvent(
                                     EventType::kMouseReleased, button, x, y)));
  }
}

TEST_F(InputInjectorEvdevTest, LeftClick) {
  ExpectClick(12, 13, EF_LEFT_MOUSE_BUTTON, 1);
  SimulateMouseClick(12, 13, EF_LEFT_MOUSE_BUTTON, 1);
  run_loop_.RunUntilIdle();
}

TEST_F(InputInjectorEvdevTest, RightClick) {
  ExpectClick(12, 13, EF_RIGHT_MOUSE_BUTTON, 1);
  SimulateMouseClick(12, 13, EF_RIGHT_MOUSE_BUTTON, 1);
  run_loop_.RunUntilIdle();
}

TEST_F(InputInjectorEvdevTest, MiddleClick) {
  ExpectClick(12, 13, EF_MIDDLE_MOUSE_BUTTON, 1);
  SimulateMouseClick(12, 13, EF_MIDDLE_MOUSE_BUTTON, 1);
  run_loop_.RunUntilIdle();
}

TEST_F(InputInjectorEvdevTest, DoubleClick) {
  ExpectClick(12, 13, EF_LEFT_MOUSE_BUTTON, 2);
  SimulateMouseClick(12, 13, EF_LEFT_MOUSE_BUTTON, 2);
  run_loop_.RunUntilIdle();
}

TEST_F(InputInjectorEvdevTest, MouseMoved) {
  injector_.MoveCursorTo(gfx::PointF(1, 1));
  run_loop_.RunUntilIdle();
  EXPECT_EQ(cursor_.GetLocation(), gfx::PointF(1, 1));
}

TEST_F(InputInjectorEvdevTest, MouseDragged) {
  InSequence dummy;
  EXPECT_CALL(event_observer_,
              OnMouseEvent(MatchesMouseEvent(EventType::kMousePressed,
                                             EF_LEFT_MOUSE_BUTTON, 0, 0)));
  EXPECT_CALL(event_observer_,
              OnMouseEvent(MatchesMouseEvent(EventType::kMouseDragged,
                                             EF_LEFT_MOUSE_BUTTON, 1, 1)));
  EXPECT_CALL(event_observer_,
              OnMouseEvent(MatchesMouseEvent(EventType::kMouseDragged,
                                             EF_LEFT_MOUSE_BUTTON, 2, 3)));
  EXPECT_CALL(event_observer_,
              OnMouseEvent(MatchesMouseEvent(EventType::kMouseReleased,
                                             EF_LEFT_MOUSE_BUTTON, 2, 3)));
  injector_.InjectMouseButton(EF_LEFT_MOUSE_BUTTON, true);
  injector_.MoveCursorTo(gfx::PointF(1, 1));
  injector_.MoveCursorTo(gfx::PointF(2, 3));
  injector_.InjectMouseButton(EF_LEFT_MOUSE_BUTTON, false);
  run_loop_.RunUntilIdle();
}

TEST_F(InputInjectorEvdevTest, MouseWheel) {
  InSequence dummy;
  EXPECT_CALL(event_observer_,
              OnMouseWheelEvent(
                  AllOf(MatchesMouseEvent(EventType::kMousewheel, 0, 10, 20),
                        Property(&MouseWheelEvent::x_offset, 0),
                        Property(&MouseWheelEvent::y_offset, 100))));
  EXPECT_CALL(event_observer_,
              OnMouseWheelEvent(
                  AllOf(MatchesMouseEvent(EventType::kMousewheel, 0, 10, 20),
                        Property(&MouseWheelEvent::x_offset, 100),
                        Property(&MouseWheelEvent::y_offset, 0))));
  injector_.MoveCursorTo(gfx::PointF(10, 20));
  injector_.InjectMouseWheel(0, 100);
  injector_.InjectMouseWheel(100, 0);
  run_loop_.RunUntilIdle();
}

}  // namespace ui
