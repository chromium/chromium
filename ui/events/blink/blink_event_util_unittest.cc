// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_event_util.h"

#include <array>
#include <limits>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"

namespace ui {

using BlinkEventUtilTest = testing::Test;

TEST(BlinkEventUtilTest, DebuggerOriginRoundTrips) {
  for (bool enabled : {false, true}) {
    SCOPED_TRACE(enabled);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(features::kPreserveDebuggerInputOrigin,
                                      enabled);
    // Check this direction independently, including when a UI event already
    // carries the flag and the feature is disabled.
    EXPECT_EQ(enabled, (EventFlagsToWebEventModifiers(EF_FROM_DEBUGGER) &
                        blink::WebInputEvent::kFromDebugger) != 0);

    for (bool from_debugger : {false, true}) {
      SCOPED_TRACE(from_debugger);
      const int modifiers =
          blink::WebInputEvent::kShiftKey |
          (from_debugger ? blink::WebInputEvent::kFromDebugger
                         : blink::WebInputEvent::kNoModifiers);
      const int flags = WebEventModifiersToEventFlags(modifiers);
      const bool expected_origin = enabled && from_debugger;
      EXPECT_EQ(expected_origin, (flags & EF_FROM_DEBUGGER) != 0);
      auto expect_origin = [&](const blink::WebInputEvent& event) {
        EXPECT_EQ(expected_origin, (event.GetModifiers() &
                                    blink::WebInputEvent::kFromDebugger) != 0)
            << blink::WebInputEvent::GetName(event.GetType());
        EXPECT_TRUE(event.GetModifiers() & blink::WebInputEvent::kShiftKey);
      };

      const gfx::PointF location(1, 2);
      const base::TimeTicks time = base::TimeTicks::Now();
      MouseEvent mouse(EventType::kMousePressed, location, location, time,
                       flags | EF_LEFT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON);
      expect_origin(MakeWebMouseEvent(mouse));
      MouseWheelEvent wheel(gfx::Vector2d(0, 1), location, location, time,
                            flags, 0);
      expect_origin(MakeWebMouseWheelEvent(wheel));
      ScrollEvent fling(EventType::kScrollFlingStart, location, location, time,
                        flags, 0, 1, 0, 1, 2);
      expect_origin(MakeWebGestureEvent(fling));
      GestureEventDetails details(EventType::kGesturePinchBegin);
      details.set_device_type(GestureDeviceType::DEVICE_TOUCHPAD);
      GestureEvent pinch(1, 2, flags, time, details);
      expect_origin(MakeWebGestureEvent(pinch));
    }
  }
}

TEST(BlinkEventUtilTest, NoScalingWith1DSF) {
  ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  details.set_scroll_update_rails_mode(ui::GestureScrollRailsMode::kHorizontal);
  auto event =
      CreateWebGestureEvent(details,
                            base::TimeTicks(),
                            gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f),
                            0,
                            0U);
  EXPECT_EQ(ui::GestureScrollRailsMode::kHorizontal,
            event.data.scroll_update.rails_mode);
  EXPECT_FALSE(ScaleWebInputEvent(event, 1.f));
  EXPECT_TRUE(ScaleWebInputEvent(event, 2.f));
}

TEST(BlinkEventUtilTest, NonPaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.delta_units = ui::ScrollGranularity::kScrollByPixel;
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
  details.set_scroll_begin_rails_mode(ui::GestureScrollRailsMode::kVertical);
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
  EXPECT_EQ(ui::GestureScrollRailsMode::kVertical,
            gestureEvent->data.scroll_begin.rails_mode);
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
  float nan_number = std::numeric_limits<float>::quiet_NaN();
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
  static const auto types = std::to_array<ui::EventType>({
      ui::EventType::kGestureScrollBegin,
      ui::EventType::kGestureScrollUpdate,
  });

  static const auto units = std::to_array<ui::ScrollGranularity>({
      ui::ScrollGranularity::kScrollByLine,
      ui::ScrollGranularity::kScrollByDocument,
  });

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
                                              WebInputEvent::kIsAutoRepeat,
                                              WebInputEvent::kFromDebugger};
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

TEST(BlinkEventUtilTest, FlingStartRailsModePlumbing) {
  ui::GestureEventDetails details(ui::EventType::kScrollFlingStart, 10.f, 20.f);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  details.set_fling_rails_mode(ui::GestureScrollRailsMode::kVertical);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  EXPECT_EQ(ui::GestureScrollRailsMode::kVertical,
            event.data.fling_start.rails_mode);
}

}  // namespace ui
