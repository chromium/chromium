// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/blink/blink_event_util.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "ui/events/gesture_event_details.h"

namespace ui {

using BlinkEventUtilTest = testing::Test;

TEST(BlinkEventUtilTest, NoScalingWith1DSF) {
  ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate, 1, 1);
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

void RunTest(ui::ScrollGranularity granularity) {
  blink::WebMouseWheelEvent event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.delta_units = granularity;
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

TEST(BlinkEventUtilTest, NonPaginatedWebMouseWheelEvent) {
  RunTest(ui::ScrollGranularity::kScrollByPixel);
}

TEST(BlinkEventUtilTest, NonPaginatedWebMouseWheelEventPercentBased) {
  RunTest(ui::ScrollGranularity::kScrollByPercentage);
}

TEST(BlinkEventUtilTest, PaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.delta_units = ui::ScrollGranularity::kScrollByPage;
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
  ui::GestureEventDetails details(ui::EventType::kGestureScrollBegin, 1, 1);
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
  ui::GestureEventDetails details(ui::EventType::kGestureScrollBegin, 1, 1,
                                  ui::ScrollGranularity::kScrollByPage);
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

TEST(BlinkEventUtilTest, EnsureFlingVelocityNotNaN) {
  float nan_number = std::nanf("");
  ui::GestureEventDetails details(ui::EventType::kScrollFlingStart, nan_number,
                                  1.f);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  EXPECT_EQ(std::numeric_limits<float>::max(),
            event.data.fling_start.velocity_x);
  EXPECT_EQ(1.f, event.data.fling_start.velocity_y);
}

TEST(BlinkEventUtilTest, NonPaginatedScrollUpdateEvent) {
  ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate, 1, 1);
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
  ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate, 1, 1,
                                  ui::ScrollGranularity::kScrollByPage);
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
      ui::EventType::kGestureScrollBegin,
      ui::EventType::kGestureScrollUpdate,
  };

  static const ui::ScrollGranularity units[] = {
      ui::ScrollGranularity::kScrollByLine,
      ui::ScrollGranularity::kScrollByDocument,
  };

  for (size_t i = 0; i < std::size(types); i++) {
    ui::EventType type = types[i];
    for (size_t j = 0; j < std::size(units); j++) {
      ui::ScrollGranularity unit = units[j];
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
      if (type == ui::EventType::kGestureScrollBegin) {
        EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_x_hint);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_y_hint);
      } else {
        EXPECT_TRUE(type == ui::EventType::kGestureScrollUpdate);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_x);
        EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_y);
      }
    }
  }
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
