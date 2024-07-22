// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <wayland-server.h>
#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zcr_touch_stylus.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "base/memory/free_deleter.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

using ::testing::_;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

namespace {

ACTION_P(CloneEvent, ptr) {
  *ptr = arg0->Clone();
}

}  // namespace

class WaylandTouchTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(
          server->seat()->resource(),
          WL_SEAT_CAPABILITY_TOUCH | WL_SEAT_CAPABILITY_KEYBOARD);
    });

    ASSERT_TRUE(connection_->seat()->touch());
    ASSERT_TRUE(connection_->seat()->keyboard());

    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetKeyboardDevices().size());
    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetTouchscreenDevices().size());
  }

 protected:
  void CheckEventType(
      ui::EventType event_type,
      ui::Event* event,
      ui::EventPointerType pointer_type = ui::EventPointerType::kTouch,
      float force = std::numeric_limits<float>::quiet_NaN(),
      float tilt_x = 0.0,
      float tilt_y = 0.0) {
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());

    auto* touch_event = event->AsTouchEvent();
    EXPECT_EQ(event_type, touch_event->type());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // These checks rely on the Exo-only protocol zcr_touch_stylus_v2 [1]
    // at //t_p/wayland-protocols/unstable/stylus/stylus-unstable-v2.xml
    auto compare_float = [](float a, float b) -> bool {
      constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
      return std::isnan(a) ? std::isnan(b) : fabs(a - b) < kEpsilon;
    };

    EXPECT_EQ(pointer_type, touch_event->pointer_details().pointer_type);
    EXPECT_TRUE(compare_float(force, touch_event->pointer_details().force));
    EXPECT_TRUE(compare_float(tilt_x, touch_event->pointer_details().tilt_x));
    EXPECT_TRUE(compare_float(tilt_y, touch_event->pointer_details().tilt_y));
#endif
  }
};

TEST_F(WaylandTouchTest, TouchPressAndMotion) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, 0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchPressed, event.get());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_motion(touch, server->GetNextTime(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchMoved, event.get());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     0 /* id */);
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchReleased, event.get());
}

// Tests that touch events with stylus pen work.
TEST_F(WaylandTouchTest, TouchPressAndMotionWithStylus) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const stylus = server->seat()->touch()->touch_stylus()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    zcr_touch_stylus_v2_send_tool(stylus, 0 /* id */,
                                  ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN);

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, 0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchPressed, event.get(),
                 ui::EventPointerType::kPen);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_motion(touch, server->GetNextTime(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchMoved, event.get(),
                 ui::EventPointerType::kPen);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     0 /* id */);
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchReleased, event.get(),
                 ui::EventPointerType::kPen);
}

// Tests that touch events with stylus pen work. This variant of the test sends
// the tool information after the touch down event, and ensures that
// wl_touch::frame event handles it correctly.
TEST_F(WaylandTouchTest, TouchPressAndMotionWithStylus2) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const stylus = server->seat()->touch()->touch_stylus()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, 0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    zcr_touch_stylus_v2_send_tool(stylus, 0 /* id */,
                                  ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN);
    zcr_touch_stylus_v2_send_force(stylus, server->GetNextTime(), 0 /* id */,
                                   wl_fixed_from_double(1.0f));
    zcr_touch_stylus_v2_send_tilt(stylus, server->GetNextTime(), 0 /* id */,
                                  wl_fixed_from_double(-45),
                                  wl_fixed_from_double(45));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchPressed, event.get(),
                 ui::EventPointerType::kPen, 1.0f /* force */,
                 -45.0f /* tilt_x */, 45.0f /* tilt_y */);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_motion(touch, server->GetNextTime(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchMoved, event.get(),
                 ui::EventPointerType::kPen, 1.0f /* force */,
                 -45.0f /* tilt_x */, 45.0f /* tilt_y */);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     0 /* id */);
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchReleased, event.get(),
                 ui::EventPointerType::kPen, 1.0f /* force */,
                 -45.0f /* tilt_x */, 45.0f /* tilt_y */);
}

// Tests that touch focus is correctly set and released.
TEST_F(WaylandTouchTest, CheckTouchFocus) {
  constexpr uint32_t touch_id1 = 1;
  constexpr uint32_t touch_id2 = 2;
  constexpr uint32_t touch_id3 = 3;

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id1, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     touch_id1);
    wl_touch_send_frame(touch);
  });

  EXPECT_FALSE(window_->has_touch_focus());

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id1, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id2, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);
    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id3, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     touch_id2);
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     touch_id1);
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     touch_id3);
    wl_touch_send_frame(touch);
  });

  EXPECT_FALSE(window_->has_touch_focus());

  // Now send many touches and cancel them.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id1, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id2, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, touch_id3, wl_fixed_from_int(30),
                       wl_fixed_from_int(40));
    wl_touch_send_frame(touch);
  });

  EXPECT_TRUE(window_->has_touch_focus());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_cancel(touch);
  });

  EXPECT_FALSE(window_->has_touch_focus());
}

// Verifies keyboard modifier flags are set in touch events while modifier keys
// are pressed. Regression test for https://crbug.com/1298604.
TEST_F(WaylandTouchTest, KeyboardFlagsSet) {
  std::unique_ptr<Event> event;

  MaybeSetUpXkb();

  // Press 'control' key.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               4 /* mods_depressed*/, 0 /* mods_latched */,
                               0 /* mods_locked */, 0 /* group */);
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 29 /* Control */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, 0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchPressed, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_motion(touch, server->GetNextTime(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchMoved, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     0 /* id */);
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchReleased, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  // Release 'control' key.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const touch = server->seat()->touch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               0 /* mods_depressed*/, 0 /* mods_latched */,
                               0 /* mods_locked */, 0 /* group */);
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 29 /* Control */,
                         WL_KEYBOARD_KEY_STATE_RELEASED);

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, 0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchPressed, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_motion(touch, server->GetNextTime(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchMoved, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_up(touch, server->GetNextSerial(), server->GetNextTime(),
                     0 /* id */);
    wl_touch_send_frame(touch);
  });

  CheckEventType(ui::EventType::kTouchReleased, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);
}

}  // namespace ui
