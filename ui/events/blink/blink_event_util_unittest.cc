// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_event_util.h"

#include "base/stl_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "ui/events/gesture_event_details.h"

namespace ui {

namespace {

blink::WebMouseEvent CreateWebMouseMoveEvent() {
  blink::WebMouseEvent mouse_event;
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.id = 1;
  mouse_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  return mouse_event;
}

blink::WebPointerEvent CreateWebPointerMoveEvent() {
  blink::WebPointerEvent pointer_event;
  pointer_event.SetType(blink::WebInputEvent::kPointerMove);
  pointer_event.id = 1;
  pointer_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  return pointer_event;
}

blink::WebTouchEvent CreateWebTouchMoveEvent() {
  blink::WebTouchPoint touch_point;
  touch_point.id = 1;
  touch_point.state = blink::WebTouchPoint::kStateMoved;
  touch_point.pointer_type = blink::WebPointerProperties::PointerType::kTouch;

  blink::WebTouchEvent touch_event;
  touch_event.SetType(blink::WebInputEvent::kTouchMove);
  touch_event.touches[touch_event.touches_length++] = touch_point;
  return touch_event;
}

}  // namespace

using BlinkEventUtilTest = testing::Test;

TEST(BlinkEventUtilTest, NoScalingWith1DSF) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details,
                            base::TimeTicks(),
                            gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f),
                            0,
                            0U);
  EXPECT_FALSE(ScaleWebInputEvent(event, 1.f));
  EXPECT_TRUE(ScaleWebInputEvent(event, 2.f));
}

TEST(BlinkEventUtilTest, NonPaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.delta_units = ui::input_types::ScrollGranularity::kScrollByPixel;
  event.delta_x = 1.f;
  event.delta_y = 1.f;
  event.wheel_ticks_x = 1.f;
  event.wheel_ticks_y = 1.f;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(2.f, mouseWheelEvent->delta_x);
  EXPECT_EQ(2.f, mouseWheelEvent->delta_y);
  EXPECT_EQ(2.f, mouseWheelEvent->wheel_ticks_x);
  EXPECT_EQ(2.f, mouseWheelEvent->wheel_ticks_y);
}

TEST(BlinkEventUtilTest, PaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.delta_units = ui::input_types::ScrollGranularity::kScrollByPage;
  event.delta_x = 1.f;
  event.delta_y = 1.f;
  event.wheel_ticks_x = 1.f;
  event.wheel_ticks_y = 1.f;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(1.f, mouseWheelEvent->delta_x);
  EXPECT_EQ(1.f, mouseWheelEvent->delta_y);
  EXPECT_EQ(1.f, mouseWheelEvent->wheel_ticks_x);
  EXPECT_EQ(1.f, mouseWheelEvent->wheel_ticks_y);
}

TEST(BlinkEventUtilTest, NonPaginatedScrollBeginEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_BEGIN, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(2.f, gestureEvent->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(2.f, gestureEvent->data.scroll_begin.delta_y_hint);
}

TEST(BlinkEventUtilTest, PaginatedScrollBeginEvent) {
  ui::GestureEventDetails details(
      ui::ET_GESTURE_SCROLL_BEGIN, 1, 1,
      ui::input_types::ScrollGranularity::kScrollByPage);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_y_hint);
}

TEST(BlinkEventUtilTest, NonPaginatedScrollUpdateEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(2.f, gestureEvent->data.scroll_update.delta_x);
  EXPECT_EQ(2.f, gestureEvent->data.scroll_update.delta_y);
}

TEST(BlinkEventUtilTest, PaginatedScrollUpdateEvent) {
  ui::GestureEventDetails details(
      ui::ET_GESTURE_SCROLL_UPDATE, 1, 1,
      ui::input_types::ScrollGranularity::kScrollByPage);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_x);
  EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_y);
}

TEST(BlinkEventUtilTest, LineAndDocumentScrollEvents) {
  static const ui::EventType types[] = {
      ui::ET_GESTURE_SCROLL_BEGIN,
      ui::ET_GESTURE_SCROLL_UPDATE,
  };

  static const ui::input_types::ScrollGranularity units[] = {
      ui::input_types::ScrollGranularity::kScrollByLine,
      ui::input_types::ScrollGranularity::kScrollByDocument,
  };

  for (size_t i = 0; i < base::size(types); i++) {
    ui::EventType type = types[i];
    for (size_t j = 0; j < base::size(units); j++) {
      ui::input_types::ScrollGranularity unit = units[j];
      ui::GestureEventDetails details(type, 1, 1, unit);
      details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
      auto event = CreateWebGestureEvent(details, base::TimeTicks(),
                                         gfx::PointF(1.f, 1.f),
                                         gfx::PointF(1.f, 1.f), 0, 0U);
      std::unique_ptr<blink::WebInputEvent> webEvent =
          ScaleWebInputEvent(event, 2.f);
      EXPECT_TRUE(webEvent);
      blink::WebGestureEvent* gestureEvent =
          static_cast<blink::WebGestureEvent*>(webEvent.get());
      // Line and document based scroll events should not be scaled.
      if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
        EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_x_hint);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_y_hint);
      } else {
        EXPECT_TRUE(type == ui::ET_GESTURE_SCROLL_UPDATE);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_x);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_y);
      }
    }
  }
}

TEST(BlinkEventUtilTest, TouchEventCoalescing) {
  blink::WebTouchEvent coalesced_event = CreateWebTouchMoveEvent();
  coalesced_event.SetType(blink::WebInputEvent::kTouchMove);
  coalesced_event.touches[0].movement_x = 5;
  coalesced_event.touches[0].movement_y = 10;

  blink::WebTouchEvent event_to_be_coalesced = CreateWebTouchMoveEvent();
  event_to_be_coalesced.touches[0].movement_x = 3;
  event_to_be_coalesced.touches[0].movement_y = -4;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(8, coalesced_event.touches[0].movement_x);
  EXPECT_EQ(6, coalesced_event.touches[0].movement_y);

  coalesced_event.touches[0].pointer_type =
      blink::WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  coalesced_event = CreateWebTouchMoveEvent();
  event_to_be_coalesced = CreateWebTouchMoveEvent();
  event_to_be_coalesced.SetModifiers(blink::WebInputEvent::kControlKey);
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, WebMouseWheelEventCoalescing) {
  blink::WebMouseWheelEvent coalesced_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  coalesced_event.delta_x = 1;
  coalesced_event.delta_y = 1;

  blink::WebMouseWheelEvent event_to_be_coalesced(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event_to_be_coalesced.delta_x = 3;
  event_to_be_coalesced.delta_y = 4;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(4, coalesced_event.delta_x);
  EXPECT_EQ(5, coalesced_event.delta_y);

  event_to_be_coalesced.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  coalesced_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // With timer based wheel scroll latching, we break the latching sequence on
  // direction change when all prior GSU events in the current sequence are
  // ignored. To do so we dispatch the pending wheel event with phaseEnded and
  // the first wheel event in the opposite direction will have phaseBegan. The
  // GSB generated from this wheel event will cause a new hittesting. To make
  // sure that a GSB will actually get created we should not coalesce the wheel
  // event with synthetic kPhaseBegan to one with synthetic kPhaseEnded.
  event_to_be_coalesced.has_synthetic_phase = true;
  coalesced_event.has_synthetic_phase = true;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  event_to_be_coalesced.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  coalesced_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, coalesced_event.phase);
  EXPECT_EQ(7, coalesced_event.delta_x);
  EXPECT_EQ(9, coalesced_event.delta_y);

  event_to_be_coalesced.resending_plugin_id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, WebGestureEventCoalescing) {
  blink::WebGestureEvent coalesced_event(
      blink::WebInputEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  coalesced_event.data.scroll_update.delta_x = 1;
  coalesced_event.data.scroll_update.delta_y = 1;

  blink::WebGestureEvent event_to_be_coalesced(
      blink::WebInputEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event_to_be_coalesced.data.scroll_update.delta_x = 3;
  event_to_be_coalesced.data.scroll_update.delta_y = 4;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(4, coalesced_event.data.scroll_update.delta_x);
  EXPECT_EQ(5, coalesced_event.data.scroll_update.delta_y);

  event_to_be_coalesced.resending_plugin_id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, GesturePinchUpdateCoalescing) {
  gfx::PointF position(10.f, 10.f);
  blink::WebGestureEvent coalesced_event(
      blink::WebInputEvent::kGesturePinchUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  coalesced_event.data.pinch_update.scale = 1.1f;
  coalesced_event.SetPositionInWidget(position);

  blink::WebGestureEvent event_to_be_coalesced(coalesced_event);

  ASSERT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_FLOAT_EQ(1.21, coalesced_event.data.pinch_update.scale);

  // Allow the updates to be coalesced if the anchors are nearly equal.
  position.Offset(0.1f, 0.1f);
  event_to_be_coalesced.SetPositionInWidget(position);
  coalesced_event.data.pinch_update.scale = 1.1f;
  ASSERT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_FLOAT_EQ(1.21, coalesced_event.data.pinch_update.scale);

  // The anchors are no longer considered equal, so don't coalesce.
  position.Offset(1.f, 1.f);
  event_to_be_coalesced.SetPositionInWidget(position);
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Don't logically coalesce touchpad pinch events as touchpad pinch events
  // don't occur within a gesture scroll sequence.
  EXPECT_FALSE(
      IsCompatibleScrollorPinch(event_to_be_coalesced, coalesced_event));

  // Touchscreen pinch events can be logically coalesced.
  coalesced_event.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  event_to_be_coalesced.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  coalesced_event.data.pinch_update.scale = 1.1f;
  ASSERT_TRUE(
      IsCompatibleScrollorPinch(event_to_be_coalesced, coalesced_event));

  blink::WebGestureEvent logical_scroll, logical_pinch;
  std::tie(logical_scroll, logical_pinch) =
      CoalesceScrollAndPinch(nullptr, coalesced_event, event_to_be_coalesced);
  ASSERT_EQ(blink::WebInputEvent::kGestureScrollUpdate,
            logical_scroll.GetType());
  ASSERT_EQ(blink::WebInputEvent::kGesturePinchUpdate, logical_pinch.GetType());
  EXPECT_FLOAT_EQ(1.21, logical_pinch.data.pinch_update.scale);
}

TEST(BlinkEventUtilTest, MouseEventCoalescing) {
  blink::WebMouseEvent coalesced_event = CreateWebMouseMoveEvent();
  blink::WebMouseEvent event_to_be_coalesced = CreateWebMouseMoveEvent();
  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test coalescing movements.
  coalesced_event.movement_x = 5;
  coalesced_event.movement_y = 10;

  event_to_be_coalesced.movement_x = 3;
  event_to_be_coalesced.movement_y = -4;
  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(8, coalesced_event.movement_x);
  EXPECT_EQ(6, coalesced_event.movement_y);

  // Test id.
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test pointer_type.
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.pointer_type =
      blink::WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test modifiers
  coalesced_event = CreateWebMouseMoveEvent();
  event_to_be_coalesced = CreateWebMouseMoveEvent();
  event_to_be_coalesced.SetModifiers(blink::WebInputEvent::kControlKey);
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, PointerEventCoalescing) {
  blink::WebPointerEvent coalesced_event = CreateWebPointerMoveEvent();
  blink::WebPointerEvent event_to_be_coalesced = CreateWebPointerMoveEvent();
  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test coalescing movements.
  coalesced_event.movement_x = 5;
  coalesced_event.movement_y = 10;

  event_to_be_coalesced.movement_x = 3;
  event_to_be_coalesced.movement_y = -4;
  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(8, coalesced_event.movement_x);
  EXPECT_EQ(6, coalesced_event.movement_y);

  // Test id.
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test pointer_type.
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.pointer_type =
      blink::WebPointerProperties::PointerType::kPen;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));

  // Test modifiers
  coalesced_event = CreateWebPointerMoveEvent();
  event_to_be_coalesced = CreateWebPointerMoveEvent();
  event_to_be_coalesced.SetModifiers(blink::WebInputEvent::kControlKey);
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, WebEventModifersAndEventFlags) {
  using WebInputEvent = blink::WebInputEvent;
  constexpr int kWebEventModifiersToTest[] = {WebInputEvent::kShiftKey,
                                              WebInputEvent::kControlKey,
                                              WebInputEvent::kAltKey,
                                              WebInputEvent::kAltGrKey,
                                              WebInputEvent::kMetaKey,
                                              WebInputEvent::kCapsLockOn,
                                              WebInputEvent::kNumLockOn,
                                              WebInputEvent::kScrollLockOn,
                                              WebInputEvent::kLeftButtonDown,
                                              WebInputEvent::kMiddleButtonDown,
                                              WebInputEvent::kRightButtonDown,
                                              WebInputEvent::kBackButtonDown,
                                              WebInputEvent::kForwardButtonDown,
                                              WebInputEvent::kIsAutoRepeat};
  // For each WebEventModifier value, test that it maps to a unique ui::Event
  // flag, and that the flag correctly maps back to the WebEventModifier.
  int event_flags = 0;
  for (int event_modifier : kWebEventModifiersToTest) {
    int event_flag = WebEventModifiersToEventFlags(event_modifier);

    // |event_flag| must be unique.
    EXPECT_EQ(event_flags & event_flag, 0);
    event_flags |= event_flag;

    // |event_flag| must map to |event_modifier|.
    EXPECT_EQ(EventFlagsToWebEventModifiers(event_flag), event_modifier);
  }
}

}  // namespace ui
