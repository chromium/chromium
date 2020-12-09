// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>

#include <cmath>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::SaveArg;

namespace ui {

class WaylandPointerTest : public WaylandTest {
 public:
  WaylandPointerTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);

    Sync();

    pointer_ = server_.seat()->pointer();
    ASSERT_TRUE(pointer_);
  }

 protected:
  wl::MockPointer* pointer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandPointerTest);
};

void SendAxisEvents(struct wl_resource* resource,
                    uint32_t time_ms,
                    uint32_t axis_source,
                    uint32_t axis,
                    int offset) {
  wl_pointer_send_axis_source(resource, axis_source);
  wl_pointer_send_axis(resource, time_ms, axis, wl_fixed_from_int(offset));
  wl_pointer_send_frame(resource);
}

void SendDiagonalAxisEvents(struct wl_resource* resource,
                            uint32_t time_ms,
                            uint32_t axis_source,
                            int offset_x,
                            int offset_y) {
  wl_pointer_send_axis_source(resource, axis_source);
  wl_pointer_send_axis(resource, time_ms, WL_POINTER_AXIS_VERTICAL_SCROLL,
                       wl_fixed_from_int(offset_y));
  wl_pointer_send_axis(resource, time_ms, WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                       wl_fixed_from_int(offset_x));
  wl_pointer_send_frame(resource);
}

void SendAxisStopEvents(struct wl_resource* resource, uint32_t time) {
  wl_pointer_send_axis_stop(resource, time, WL_POINTER_AXIS_VERTICAL_SCROLL);
  wl_pointer_send_axis_stop(resource, time, WL_POINTER_AXIS_HORIZONTAL_SCROLL);
  wl_pointer_send_frame(resource);
}

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_P(WaylandPointerTest, Enter) {
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(), 0, 0);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(ET_MOUSE_ENTERED, mouse_event->type());
  EXPECT_EQ(0, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(0, 0), mouse_event->location_f());
}

TEST_P(WaylandPointerTest, Leave) {
  MockPlatformWindowDelegate other_delegate;
  gfx::AcceleratedWidget other_widget = gfx::kNullAcceleratedWidget;
  EXPECT_CALL(other_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&other_widget));

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 10, 10);
  properties.type = PlatformWindowType::kWindow;
  auto other_window = WaylandWindow::Create(&other_delegate, connection_.get(),
                                            std::move(properties));
  ASSERT_NE(other_widget, gfx::kNullAcceleratedWidget);

  Sync();

  wl::MockSurface* other_surface = server_.GetObject<wl::MockSurface>(
      other_window->root_surface()->GetSurfaceId());
  ASSERT_TRUE(other_surface);

  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(), 0, 0);
  wl_pointer_send_leave(pointer_->resource(), 2, surface_->resource());
  wl_pointer_send_enter(pointer_->resource(), 3, other_surface->resource(), 0,
                        0);
  wl_pointer_send_button(pointer_->resource(), 4, 1004, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);

  // Do an extra Sync() here so that we process the second enter event before we
  // destroy |other_window|.
  Sync();
}

ACTION_P3(CloneEventAndCheckCapture, window, result, ptr) {
  ASSERT_TRUE(window->HasCapture() == result);
  *ptr = Event::Clone(*arg0);
}

TEST_P(WaylandPointerTest, Motion) {
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(), 0, 0);
  Sync();  // We're interested in checking Motion event in this test case, so
           // skip Enter event here.

  wl_pointer_send_motion(pointer_->resource(), 1002,
                         wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(ET_MOUSE_MOVED, mouse_event->type());
  EXPECT_EQ(0, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(10.75, 20.375), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(10.75, 20.375), mouse_event->root_location_f());
}

TEST_P(WaylandPointerTest, MotionDragged) {
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(), 0, 0);
  wl_pointer_send_button(pointer_->resource(), 2, 1002, BTN_MIDDLE,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  wl_pointer_send_motion(pointer_->resource(), 1003, wl_fixed_from_int(400),
                         wl_fixed_from_int(500));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(ET_MOUSE_DRAGGED, mouse_event->type());
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->root_location_f());
}

TEST_P(WaylandPointerTest, AxisVertical) {
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(),
                        wl_fixed_from_int(0), wl_fixed_from_int(0));
  wl_pointer_send_button(pointer_->resource(), 2, 1002, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  // Wayland servers typically send a value of 10 per mouse wheel click.
  wl_pointer_send_axis(pointer_->resource(), 1003,
                       WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(20));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseWheelEvent());
  auto* mouse_wheel_event = event->AsMouseWheelEvent();
  EXPECT_EQ(gfx::Vector2d(0, -2 * MouseWheelEvent::kWheelDelta),
            mouse_wheel_event->offset());
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, mouse_wheel_event->button_flags());
  EXPECT_EQ(0, mouse_wheel_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(), mouse_wheel_event->location_f());
  EXPECT_EQ(gfx::PointF(), mouse_wheel_event->root_location_f());
}

TEST_P(WaylandPointerTest, AxisHorizontal) {
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer_->resource(), 2, 1002, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  // Wayland servers typically send a value of 10 per mouse wheel click.
  wl_pointer_send_axis(pointer_->resource(), 1003,
                       WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                       wl_fixed_from_int(10));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseWheelEvent());
  auto* mouse_wheel_event = event->AsMouseWheelEvent();
  EXPECT_EQ(gfx::Vector2d(-MouseWheelEvent::kWheelDelta, 0),
            mouse_wheel_event->offset());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, mouse_wheel_event->button_flags());
  EXPECT_EQ(0, mouse_wheel_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(50, 75), mouse_wheel_event->location_f());
  EXPECT_EQ(gfx::PointF(50, 75), mouse_wheel_event->root_location_f());
}

TEST_P(WaylandPointerTest, SetBitmap) {
  SkBitmap dummy_cursor;
  dummy_cursor.setInfo(
      SkImageInfo::Make(16, 16, kUnknown_SkColorType, kUnknown_SkAlphaType));

  EXPECT_CALL(*pointer_, SetCursor(nullptr, 0, 0));
  connection_->SetCursorBitmap({}, {}, 1.0);
  connection_->ScheduleFlush();
  Sync();
  Mock::VerifyAndClearExpectations(pointer_);

  EXPECT_CALL(*pointer_, SetCursor(Ne(nullptr), 5, 8));
  connection_->SetCursorBitmap({dummy_cursor}, gfx::Point(5, 8), 1.0);
  connection_->ScheduleFlush();
  Sync();
  Mock::VerifyAndClearExpectations(pointer_);
}

TEST_P(WaylandPointerTest, SetBitmapOnPointerFocus) {
  SkBitmap dummy_cursor;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  dummy_cursor.allocPixels(info, 10 * 4);

  BitmapCursorFactoryOzone cursor_factory;
  PlatformCursor cursor = cursor_factory.CreateImageCursor(
      mojom::CursorType::kCustom, dummy_cursor, gfx::Point(5, 8));
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);

  EXPECT_CALL(*pointer_, SetCursor(Ne(nullptr), 5, 8));
  window_->SetCursor(cursor);
  connection_->ScheduleFlush();

  Sync();

  Mock::VerifyAndClearExpectations(pointer_);

  EXPECT_CALL(*pointer_, SetCursor(Ne(nullptr), 5, 8));
  wl_pointer_send_enter(pointer_->resource(), 1, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));

  Sync();

  connection_->ScheduleFlush();

  Sync();

  Mock::VerifyAndClearExpectations(pointer_);
}

TEST_P(WaylandPointerTest, FlingVertical) {
  uint32_t serial = 0;
  uint32_t time = 1001;
  wl_pointer_send_enter(pointer_->resource(), ++serial, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer_->resource(), ++serial, ++time, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));
  // 1st axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // 2nd axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents(pointer_->resource(), ++time);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsMouseWheelEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsMouseWheelEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event = event3->AsScrollEvent();
  EXPECT_EQ(ET_SCROLL_FLING_START, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  EXPECT_EQ(0.0f, scroll_event->x_offset());
  EXPECT_EQ(0.0f, scroll_event->x_offset_ordinal());
  // Initial vertical velocity depends on the implementation outside of
  // WaylandPointer, but it should be negative value based on the direction of
  // recent two axis events.
  EXPECT_GT(0.0f, scroll_event->y_offset());
  EXPECT_GT(0.0f, scroll_event->y_offset_ordinal());
}

TEST_P(WaylandPointerTest, FlingHorizontal) {
  uint32_t serial = 0;
  uint32_t time = 1001;
  wl_pointer_send_enter(pointer_->resource(), ++serial, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer_->resource(), ++serial, ++time, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));
  // 1st axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_HORIZONTAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // 2nd axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_HORIZONTAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents(pointer_->resource(), ++time);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsMouseWheelEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsMouseWheelEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event = event3->AsScrollEvent();
  EXPECT_EQ(ET_SCROLL_FLING_START, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  EXPECT_EQ(0.0f, scroll_event->y_offset());
  EXPECT_EQ(0.0f, scroll_event->y_offset_ordinal());
  // Initial horizontal velocity depends on the implementation outside of
  // WaylandPointer, but it should be negative value based on the direction of
  // recent two axis events.
  EXPECT_GT(0.0f, scroll_event->x_offset());
  EXPECT_GT(0.0f, scroll_event->x_offset_ordinal());
}

TEST_P(WaylandPointerTest, FlingCancel) {
  uint32_t serial = 0;
  uint32_t time = 1001;
  wl_pointer_send_enter(pointer_->resource(), ++serial, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer_->resource(), ++serial, ++time, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event1, event2, event3, event4;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(4)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3))
      .WillOnce(CloneEvent(&event4));
  // 1st axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // 2nd axis event.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // 3rd axis event, whose offset is 0, should make the following axis_stop
  // trigger fling cancel.
  SendAxisEvents(pointer_->resource(), ++time, WL_POINTER_AXIS_SOURCE_FINGER,
                 WL_POINTER_AXIS_VERTICAL_SCROLL, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // axis_stop event which should trigger fling cancel.
  SendAxisStopEvents(pointer_->resource(), ++time);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsMouseWheelEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsMouseWheelEvent());

  // The 3rd axis event's offset is 0.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsMouseWheelEvent());
  auto* mouse_wheel_event = event3->AsMouseWheelEvent();
  EXPECT_EQ(gfx::Vector2d(0, 0), mouse_wheel_event->offset());

  // The 4th event should be FLING_CANCEL.
  ASSERT_TRUE(event4);
  ASSERT_TRUE(event4->IsScrollEvent());
  auto* scroll_event = event4->AsScrollEvent();
  EXPECT_EQ(ET_SCROLL_FLING_CANCEL, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  EXPECT_EQ(0.0f, scroll_event->x_offset());
  EXPECT_EQ(0.0f, scroll_event->y_offset());
  EXPECT_EQ(0.0f, scroll_event->x_offset_ordinal());
  EXPECT_EQ(0.0f, scroll_event->y_offset_ordinal());
}

TEST_P(WaylandPointerTest, FlingDiagonal) {
  uint32_t serial = 0;
  uint32_t time = 1001;
  wl_pointer_send_enter(pointer_->resource(), ++serial, surface_->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer_->resource(), ++serial, ++time, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event1, event2, event3, event4, event5;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(5)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3))
      .WillOnce(CloneEvent(&event4))
      .WillOnce(CloneEvent(&event5));
  // 1st axis event notifies scrolls both in vertical and horizontal.
  SendDiagonalAxisEvents(pointer_->resource(), ++time,
                         WL_POINTER_AXIS_SOURCE_FINGER, 20, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // 2st axis event notifies scrolls both in vertical and horizontal.
  SendDiagonalAxisEvents(pointer_->resource(), ++time,
                         WL_POINTER_AXIS_SOURCE_FINGER, 20, 10);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents(pointer_->resource(), ++time);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  Sync();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsMouseWheelEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsMouseWheelEvent());
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsMouseWheelEvent());
  ASSERT_TRUE(event4);
  ASSERT_TRUE(event4->IsMouseWheelEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event5);
  ASSERT_TRUE(event5->IsScrollEvent());
  auto* scroll_event = event5->AsScrollEvent();
  EXPECT_EQ(ET_SCROLL_FLING_START, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  // Check the offset direction. It should non-zero in both axes.
  EXPECT_GT(0.0f, scroll_event->x_offset());
  EXPECT_GT(0.0f, scroll_event->y_offset());
  EXPECT_GT(0.0f, scroll_event->x_offset_ordinal());
  EXPECT_GT(0.0f, scroll_event->y_offset_ordinal());
  // Horizontal offset should be larger than vertical one, given the scroll
  // offset in each direction.
  EXPECT_GT(std::abs(scroll_event->x_offset()),
            std::abs(scroll_event->y_offset()));
  EXPECT_GT(std::abs(scroll_event->x_offset_ordinal()),
            std::abs(scroll_event->y_offset_ordinal()));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandPointerTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandPointerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
