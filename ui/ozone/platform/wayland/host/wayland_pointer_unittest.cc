// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>

#include <cmath>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_pointer_stylus.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

class WaylandPointerTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_POINTER);
    });
    ASSERT_TRUE(connection_->seat()->pointer());

    EXPECT_EQ(1u, DeviceDataManager::GetInstance()->GetMouseDevices().size());
    // Wayland doesn't expose touchpad devices separately. They are all
    // WaylandPointers.
    EXPECT_EQ(0u,
              DeviceDataManager::GetInstance()->GetTouchpadDevices().size());
  }

 protected:
  // The auxiliary convenience methods for entering and leaving the pointer into
  // and from the surface.
  void SendEnter(int x, int y) {
    PostToServerAndWait(
        [x, y, surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const pointer = server->seat()->pointer()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl_pointer_send_enter(pointer, server->GetNextSerial(), surface,
                                wl_fixed_from_int(x), wl_fixed_from_int(y));
          wl_pointer_send_frame(pointer);
        });
  }
  void SendEnter() { SendEnter(0, 0); }

  void SendLeave() {
    PostToServerAndWait(
        [surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const pointer = server->seat()->pointer()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
          wl_pointer_send_frame(pointer);
        });
  }

  void SendAxisStopEvents() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();

      wl_pointer_send_axis_stop(pointer, server->GetNextTime(),
                                WL_POINTER_AXIS_VERTICAL_SCROLL);
      wl_pointer_send_axis_stop(pointer, server->GetNextTime(),
                                WL_POINTER_AXIS_HORIZONTAL_SCROLL);
      wl_pointer_send_frame(pointer);
    });
  }

  void SendRightButtonPress() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();

      wl_pointer_send_button(pointer, server->GetNextSerial(),
                             server->GetNextTime(), BTN_RIGHT,
                             WL_POINTER_BUTTON_STATE_PRESSED);
      wl_pointer_send_frame(pointer);
    });
  }

  void CheckEventType(
      ui::EventType event_type,
      ui::Event* event,
      ui::EventPointerType pointer_type = ui::EventPointerType::kMouse,
      float force = std::numeric_limits<float>::quiet_NaN(),
      float tilt_x = 0.0,
      float tilt_y = 0.0) {
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsMouseEvent());

    auto* mouse_event = event->AsMouseEvent();
    EXPECT_EQ(event_type, mouse_event->type());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // These checks rely on the Exo-only protocol zcr_pointer_stylus_v2 [1]
    // at //t_p/wayland-protocols/unstable/stylus/stylus-unstable-v2.xml
    auto compare_float = [](float a, float b) -> bool {
      constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
      return std::isnan(a) ? std::isnan(b) : fabs(a - b) < kEpsilon;
    };

    EXPECT_EQ(pointer_type, mouse_event->pointer_details().pointer_type);
    EXPECT_TRUE(compare_float(force, mouse_event->pointer_details().force));
    EXPECT_TRUE(compare_float(tilt_x, mouse_event->pointer_details().tilt_x));
    EXPECT_TRUE(compare_float(tilt_y, mouse_event->pointer_details().tilt_y));
#endif
  }
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

ACTION_P(CloneEvent, ptr) {
  *ptr = arg0->Clone();
}

TEST_F(WaylandPointerTest, Enter) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  SendEnter();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(EventType::kMouseEntered, mouse_event->type());
  EXPECT_EQ(0, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(0, 0), mouse_event->location_f());
}

TEST_F(WaylandPointerTest, Leave) {
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

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);
  EXPECT_CALL(other_delegate, DispatchEvent(_)).Times(2);

  PostToServerAndWait(
      [surface_id = window_->root_surface()->get_surface_id(),
       other_surface_id = other_window->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* const pointer = server->seat()->pointer()->resource();
        auto* const surface =
            server->GetObject<wl::MockSurface>(surface_id)->resource();
        auto* const other_surface =
            server->GetObject<wl::MockSurface>(other_surface_id)->resource();

        wl_pointer_send_enter(pointer, 1, surface, 0, 0);
        wl_pointer_send_frame(pointer);

        wl_pointer_send_leave(pointer, 2, surface);
        wl_pointer_send_frame(pointer);

        wl_pointer_send_enter(pointer, 3, other_surface, 0, 0);
        wl_pointer_send_frame(pointer);

        wl_pointer_send_button(pointer, 4, 1004, BTN_LEFT,
                               WL_POINTER_BUTTON_STATE_PRESSED);
        wl_pointer_send_frame(pointer);
      });
}

ACTION_P3(CloneEventAndCheckCapture, window, result, ptr) {
  ASSERT_TRUE(window->HasCapture() == result);
  *ptr = arg0->Clone();
}

TEST_F(WaylandPointerTest, Motion) {
  SendEnter();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_motion(pointer, 1002, wl_fixed_from_double(10.75),
                           wl_fixed_from_double(20.375));
    wl_pointer_send_frame(pointer);
  });

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(EventType::kMouseMoved, mouse_event->type());
  EXPECT_EQ(0, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(10.75, 20.375), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(10.75, 20.375), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, MotionDragged) {
  SendEnter();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_MIDDLE,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_motion(pointer, 1003, wl_fixed_from_int(400),
                           wl_fixed_from_int(500));
    wl_pointer_send_frame(pointer);
  });

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(EventType::kMouseDragged, mouse_event->type());
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, MotionDraggedWithStylus) {
  SendEnter();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    auto* const stylus =
        server->seat()->pointer()->pointer_stylus()->resource();

    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);

    // Stylus data.
    zcr_pointer_stylus_v2_send_tool(stylus,
                                    ZCR_POINTER_STYLUS_V2_TOOL_TYPE_PEN);
    zcr_pointer_stylus_v2_send_force(stylus, server->GetNextTime(),
                                     wl_fixed_from_double(1.0f));
    zcr_pointer_stylus_v2_send_tilt(stylus, server->GetNextTime(),
                                    wl_fixed_from_double(-45),
                                    wl_fixed_from_double(45));
    wl_pointer_send_frame(pointer);
  });

  CheckEventType(ui::EventType::kMousePressed, event.get(),
                 ui::EventPointerType::kPen, 1.0f /* force */,
                 -45.0f /* tilt_x */, 45.0f /* tilt_y */);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_motion(pointer, server->GetNextTime(),
                           wl_fixed_from_int(400), wl_fixed_from_int(500));
    wl_pointer_send_frame(pointer);
  });

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(EventType::kMouseDragged, mouse_event->type());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->root_location_f());
}

// Verifies whether the platform event source handles all types of axis sources.
// The actual behaviour of each axis source is not tested here.
TEST_F(WaylandPointerTest, AxisSourceTypes) {
  SendEnter();

  std::unique_ptr<Event> event1, event2, event3, event4;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(4)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3))
      .WillOnce(CloneEvent(&event4));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    for (const auto source :
         {WL_POINTER_AXIS_SOURCE_WHEEL, WL_POINTER_AXIS_SOURCE_FINGER,
          WL_POINTER_AXIS_SOURCE_CONTINUOUS,
          WL_POINTER_AXIS_SOURCE_WHEEL_TILT}) {
      SendAxisEvents(pointer, server->GetNextTime(), source,
                     WL_POINTER_AXIS_VERTICAL_SCROLL, rand() % 20);
    }
  });

  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsMouseWheelEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  ASSERT_TRUE(event4);
  ASSERT_TRUE(event4->IsMouseWheelEvent());
}

// This test ensures Ozone/Wayland does not crash when spurious
// `stylus tool` and `axis source` events are sent by the Compositor, prior
// to a pointer clicking event.
// In practice, this might happen with specific compositors, eg Exo, when a
// device wakes up from sleeping.
TEST_F(WaylandPointerTest, SpuriousAxisSourceAndStylusToolEvents) {
  SendEnter();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    auto* const stylus =
        server->seat()->pointer()->pointer_stylus()->resource();

    // Stylus data.
    zcr_pointer_stylus_v2_send_tool(stylus,
                                    ZCR_POINTER_STYLUS_V2_TOOL_TYPE_NONE);
    wl_pointer_send_frame(pointer);

    // Wheel data.
    wl_pointer_send_axis_source(pointer, WL_POINTER_AXIS_SOURCE_WHEEL);
  });

  // Button press.  This may generate more than a single event.
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_button(pointer, 2, 1, BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  // Do not validate anything, this test only ensures that no crash occurred.
}

TEST_F(WaylandPointerTest, Axis) {
  SendEnter();

  for (uint32_t axis :
       {WL_POINTER_AXIS_VERTICAL_SCROLL, WL_POINTER_AXIS_HORIZONTAL_SCROLL}) {
    for (bool send_axis_source : {false, true}) {
      std::unique_ptr<Event> event;
      EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

      PostToServerAndWait([axis, send_axis_source](
                              wl::TestWaylandServerThread* server) {
        auto* const pointer = server->seat()->pointer()->resource();

        if (send_axis_source) {
          // The axis source event is optional.  When it is not set within the
          // event frame, we assume the mouse wheel.
          wl_pointer_send_axis_source(pointer, WL_POINTER_AXIS_SOURCE_WHEEL);
        }

        // Wayland servers typically send a value of 10 per mouse wheel click.
        wl_pointer_send_axis(pointer, 1003, axis, wl_fixed_from_int(10));
        wl_pointer_send_frame(pointer);
      });

      ASSERT_TRUE(event);
      ASSERT_TRUE(event->IsMouseWheelEvent());
      auto* mouse_wheel_event = event->AsMouseWheelEvent();
      EXPECT_EQ(axis == WL_POINTER_AXIS_VERTICAL_SCROLL
                    ? gfx::Vector2d(0, -MouseWheelEvent::kWheelDelta)
                    : gfx::Vector2d(-MouseWheelEvent::kWheelDelta, 0),
                mouse_wheel_event->offset());
      EXPECT_EQ(gfx::PointF(), mouse_wheel_event->location_f());
      EXPECT_EQ(gfx::PointF(), mouse_wheel_event->root_location_f());
    }
  }
}

TEST_F(WaylandPointerTest, SetBitmap) {
  SendEnter();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer();

    EXPECT_CALL(*pointer, SetCursor(nullptr, 0, 0));
  });

  connection_->SetCursorBitmap({}, {}, 1.0);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer();

    Mock::VerifyAndClearExpectations(pointer);
  });

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer();

    EXPECT_CALL(*pointer, SetCursor(Ne(nullptr), 5, 8));
  });

  SkBitmap dummy_cursor;
  dummy_cursor.setInfo(
      SkImageInfo::Make(16, 16, kUnknown_SkColorType, kUnknown_SkAlphaType));
  connection_->SetCursorBitmap({dummy_cursor}, gfx::Point(5, 8), 1.0);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer();

    Mock::VerifyAndClearExpectations(pointer);
  });

  SendLeave();
}

// Tests that bitmap is set on pointer focus and the pointer surface respects
// provided scale of the surface image.
TEST_F(WaylandPointerTest, SetBitmapAndScaleOnPointerFocus) {
  for (float scale : {1.0, 1.2, 1.5, 1.75, 2.0, 2.5, 3.0}) {
    gfx::Size size = {static_cast<int>(10 * scale),
                      static_cast<int>(10 * scale)};
    SkBitmap dummy_cursor;
    SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                         SkColorType::kBGRA_8888_SkColorType,
                                         SkAlphaType::kPremul_SkAlphaType);
    dummy_cursor.allocPixels(info, size.width() * 4);

    const gfx::Point hotspot_px = {5, 8};
    const gfx::Point hotspot_dip =
        gfx::ScaleToRoundedPoint(hotspot_px, 1 / scale);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    BitmapCursorFactory cursor_factory;
#else
    WaylandCursorFactory cursor_factory(connection_.get());
#endif
    auto cursor = cursor_factory.CreateImageCursor(
        mojom::CursorType::kCustom, dummy_cursor, hotspot_px, scale);

    SendEnter(10, 10);

    // Set a cursor.
    wl_resource* surface_resource = nullptr;

    PostToServerAndWait([&surface_resource,
                         hotspot_dip](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer();

      // Allow up to 1 DIP of precision loss.
      EXPECT_CALL(
          *pointer,
          SetCursor(Ne(nullptr),
                    AllOf(Ge(hotspot_dip.x() - 1), Le(hotspot_dip.x() + 1)),
                    AllOf(Ge(hotspot_dip.y() - 1), Le(hotspot_dip.y() + 1))))
          .WillOnce(SaveArg<0>(&surface_resource));
    });

    window_->SetCursor(cursor);
    connection_->Flush();

    SendLeave();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer();

      Mock::VerifyAndClearExpectations(pointer);
    });

    ASSERT_TRUE(surface_resource);

    PostToServerAndWait([surface_resource, scale,
                         hotspot_dip](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer();

      auto* mock_pointer_surface =
          wl::MockSurface::FromResource(surface_resource);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      EXPECT_EQ(mock_pointer_surface->buffer_scale(), std::ceil(scale));
#else
      EXPECT_EQ(mock_pointer_surface->buffer_scale(), std::ceil(scale - 0.2f));
#endif

      // Update the focus.
      EXPECT_CALL(
          *pointer,
          SetCursor(Ne(nullptr),
                    AllOf(Ge(hotspot_dip.x() - 1), Le(hotspot_dip.x() + 1)),
                    AllOf(Ge(hotspot_dip.y() - 1), Le(hotspot_dip.y() + 1))));
    });

    SendEnter(50, 75);

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer();

      Mock::VerifyAndClearExpectations(pointer);
    });

    // Reset the focus for the next iteration.
    SendLeave();
  }
}

TEST_F(WaylandPointerTest, FlingVertical) {
  SendEnter(50, 75);

  SendRightButtonPress();

  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));

  // Send two axis events.
  for (int n = 0; n < 2; ++n) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();

      SendAxisEvents(pointer, server->GetNextTime(),
                     WL_POINTER_AXIS_SOURCE_FINGER,
                     WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
    });

    // Advance time to emulate delay between events and allow the fling gesture
    // to be recognised.
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event = event3->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  EXPECT_EQ(0.0f, scroll_event->x_offset());
  EXPECT_EQ(0.0f, scroll_event->x_offset_ordinal());
  // Initial vertical velocity depends on the implementation outside of
  // WaylandPointer, but it should be negative value based on the direction of
  // recent two axis events.
  EXPECT_GT(0.0f, scroll_event->y_offset());
  EXPECT_GT(0.0f, scroll_event->y_offset_ordinal());
}

TEST_F(WaylandPointerTest, FlingHorizontal) {
  SendEnter(50, 75);

  SendRightButtonPress();

  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));

  // Send two axis events.
  for (int n = 0; n < 2; ++n) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();

      SendAxisEvents(pointer, server->GetNextTime(),
                     WL_POINTER_AXIS_SOURCE_FINGER,
                     WL_POINTER_AXIS_HORIZONTAL_SCROLL, 10);
    });

    // Advance time to emulate delay between events and allow the fling gesture
    // to be recognised.
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event = event3->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), scroll_event->location_f());
  EXPECT_EQ(0.0f, scroll_event->y_offset());
  EXPECT_EQ(0.0f, scroll_event->y_offset_ordinal());
  // Initial horizontal velocity depends on the implementation outside of
  // WaylandPointer, but it should be negative value based on the direction of
  // recent two axis events.
  EXPECT_GT(0.0f, scroll_event->x_offset());
  EXPECT_GT(0.0f, scroll_event->x_offset_ordinal());
}

TEST_F(WaylandPointerTest, FlingCancel) {
  SendEnter(50, 75);
  SendRightButtonPress();
  std::unique_ptr<Event> event1, event2, event3, event4, event5;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(5)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3))
      .WillOnce(CloneEvent(&event4))
      .WillOnce(CloneEvent(&event5));

  // Send two axis events.
  for (int n = 0; n < 2; ++n) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();
      SendAxisEvents(pointer, server->GetNextTime(),
                     WL_POINTER_AXIS_SOURCE_FINGER,
                     WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
    });
    // Advance time to emulate delay between events and allow the fling gesture
    // to be recognised.
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  // axis_stop event which should trigger FLING_START.
  SendAxisStopEvents();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The third axis event, which simulates placing the finger on the touchpad
  // again using offset 0, should trigger a FLING_CANCEL.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    SendAxisEvents(pointer, server->GetNextTime(),
                   WL_POINTER_AXIS_SOURCE_FINGER,
                   WL_POINTER_AXIS_VERTICAL_SCROLL, 0);
  });
#endif

  // Another axis scroll event is added. In Linux, this must lead to a
  // FLING_CANCEL being triggered before a usual scroll event occurs because a
  // FLING_START was triggered beforehand.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    SendAxisEvents(pointer, server->GetNextTime(),
                   WL_POINTER_AXIS_SOURCE_FINGER,
                   WL_POINTER_AXIS_VERTICAL_SCROLL, 10);
  });

  // Advance time to emulate delay between events and allow the fling gesture
  // to be recognised.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  // Two usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());
  EXPECT_EQ(EventType::kScroll, event1->type());

  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());
  EXPECT_EQ(EventType::kScroll, event2->type());

  // The 3rd event will be fling start with vertical velocity.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* fling_start_event = event3->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, fling_start_event->type());
  EXPECT_EQ(0.0f, fling_start_event->x_offset());
  EXPECT_GT(0.0f, fling_start_event->y_offset());
  EXPECT_EQ(0.0f, fling_start_event->x_offset_ordinal());
  EXPECT_GT(0.0f, fling_start_event->y_offset_ordinal());

  // The 4th event should be FLING_CANCEL.
  ASSERT_TRUE(event4);
  ASSERT_TRUE(event4->IsScrollEvent());
  auto* fling_cancel_event = event4->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingCancel, fling_cancel_event->type());
  EXPECT_EQ(gfx::PointF(50, 75), fling_cancel_event->location_f());
  EXPECT_EQ(0.0f, fling_cancel_event->x_offset());
  EXPECT_EQ(0.0f, fling_cancel_event->y_offset());
  EXPECT_EQ(0.0f, fling_cancel_event->x_offset_ordinal());
  EXPECT_EQ(0.0f, fling_cancel_event->y_offset_ordinal());

  // The 5th event will be yet another axis event.
  ASSERT_TRUE(event5);
  ASSERT_TRUE(event5);
  EXPECT_EQ(EventType::kScroll, event5->type());
}

TEST_F(WaylandPointerTest, FlingDiagonal) {
  SendEnter(50, 75);

  SendRightButtonPress();

  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));

  // Send two axis events that notify scrolls both in vertical and horizontal.
  for (int n = 0; n < 2; ++n) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const pointer = server->seat()->pointer()->resource();

      SendDiagonalAxisEvents(pointer, server->GetNextTime(),
                             WL_POINTER_AXIS_SOURCE_FINGER, 20, 10);
    });

    // Advance time to emulate delay between events and allow the fling gesture
    // to be recognised.
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  // axis_stop event which should trigger fling scroll.
  SendAxisStopEvents();

  // Usual axis events should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());

  // The third dispatched event should be FLING_START.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event = event3->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event->type());
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

// The case tested here should not actually occur in a good implementation of
// the Wayland protocol.
TEST_F(WaylandPointerTest, FlingVelocityWithoutLeadingAxis) {
  SendEnter(50, 75);

  SendRightButtonPress();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(1)
      .WillOnce(CloneEvent(&event));

  // Send axis_stop event without leading axis event.
  SendAxisStopEvents();

  // We expect only a FLING_START event.
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsScrollEvent());
  auto* scroll_event = event->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event->type());

  // Check the offset direction. It should be zero in both axes.
  EXPECT_EQ(0.0f, scroll_event->x_offset());
  EXPECT_EQ(0.0f, scroll_event->y_offset());
  EXPECT_EQ(0.0f, scroll_event->x_offset_ordinal());
  EXPECT_EQ(0.0f, scroll_event->y_offset_ordinal());
}

TEST_F(WaylandPointerTest, FlingVelocityWithSingleLeadingAxis) {
  SendEnter(50, 75);

  SendRightButtonPress();

  std::unique_ptr<Event> event1, event2;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(2)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2));

  // Send a single axis event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    SendDiagonalAxisEvents(pointer, server->GetNextTime(),
                           WL_POINTER_AXIS_SOURCE_FINGER, -20, 10);
  });

  // Advance time to emulate delay between events and allow the fling gesture
  // to be recognised.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  // One axis event should follow before the fling event.
  SendAxisStopEvents();
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());

  // We expect a FLING_START event.
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());
  auto* scroll_event2 = event2->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event2->type());

  // Check the offset direction. Horizontal axis should be negative. Vertical
  // axis should be positive.
  EXPECT_LT(0.0f, scroll_event2->x_offset());
  EXPECT_GT(0.0f, scroll_event2->y_offset());
  EXPECT_LT(0.0f, scroll_event2->x_offset_ordinal());
  EXPECT_GT(0.0f, scroll_event2->y_offset_ordinal());
}

}  // namespace ui
