// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <wayland-server.h>
#include <cstdint>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_touch_stylus.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

namespace {

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

}  // namespace

class WaylandTouchTest : public WaylandTest {
 public:
  WaylandTouchTest() {}

  WaylandTouchTest(const WaylandTouchTest&) = delete;
  WaylandTouchTest& operator=(const WaylandTouchTest&) = delete;

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(
        server_.seat()->resource(),
        WL_SEAT_CAPABILITY_TOUCH | WL_SEAT_CAPABILITY_KEYBOARD);

    Sync();

    touch_ = server_.seat()->touch();
    ASSERT_TRUE(touch_);

    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetKeyboardDevices().size());
  }

 protected:
  void CheckEventType(
      ui::EventType event_type,
      ui::Event* event,
      ui::EventPointerType pointer_type = ui::EventPointerType::kTouch) {
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());

    auto* touch_event = event->AsTouchEvent();
    EXPECT_EQ(event_type, touch_event->type());
  }

  wl::TestTouch* touch_;
};

TEST_P(WaylandTouchTest, TouchPressAndMotion) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  wl_touch_send_down(touch_->resource(), 1, 0, surface_->resource(), 0 /* id */,
                     wl_fixed_from_int(50), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get());

  wl_touch_send_motion(touch_->resource(), 500, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get());

  wl_touch_send_up(touch_->resource(), 1, 1000, 0 /* id */);
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get());
}

// Tests that touch events with stylus pen work.
TEST_P(WaylandTouchTest, TouchPressAndMotionWithStylus) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  zcr_touch_stylus_v2_send_tool(touch_->touch_stylus()->resource(), 0 /* id */,
                                ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN);
  Sync();

  wl_touch_send_down(touch_->resource(), 1, 0, surface_->resource(), 0 /* id */,
                     wl_fixed_from_int(50), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get(), ui::EventPointerType::kPen);

  wl_touch_send_motion(touch_->resource(), 500, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get(), ui::EventPointerType::kPen);

  wl_touch_send_up(touch_->resource(), 1, 1000, 0 /* id */);
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get());
}

// Tests that touch focus is correctly set and released.
TEST_P(WaylandTouchTest, CheckTouchFocus) {
  uint32_t serial = 0;
  uint32_t time = 0;
  constexpr uint32_t touch_id1 = 1;
  constexpr uint32_t touch_id2 = 2;
  constexpr uint32_t touch_id3 = 3;

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(50), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id1);
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id2, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id3, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id2);
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id1);
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id3);
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());

  // Now send many touches and cancel them.
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id2, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id3, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_frame(touch_->resource());

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_cancel(touch_->resource());

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());
}

// Verifies keyboard modifier flags are set in touch events while modifier keys
// are pressed. Regression test for https://crbug.com/1298604.
TEST_P(WaylandTouchTest, KeyboardFlagsSet) {
  uint32_t serial = 0;
  uint32_t timestamp = 0;
  std::unique_ptr<Event> event;

  wl::TestKeyboard* keyboard = server_.seat()->keyboard();
  ASSERT_TRUE(keyboard);

  // Press 'control' key.
  wl_keyboard_send_key(keyboard->resource(), ++serial, ++timestamp,
                       29 /* Control */, WL_KEYBOARD_KEY_STATE_PRESSED);
  Sync();

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  wl_touch_send_down(touch_->resource(), ++serial, ++timestamp,
                     surface_->resource(), 0 /* id */, wl_fixed_from_int(50),
                     wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());
  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  wl_touch_send_motion(touch_->resource(), ++timestamp, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());
  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  wl_touch_send_up(touch_->resource(), ++serial, ++timestamp, 0 /* id */);
  wl_touch_send_frame(touch_->resource());
  Sync();

  CheckEventType(ui::ET_TOUCH_RELEASED, event.get());
  EXPECT_TRUE(event->flags() & ui::EF_CONTROL_DOWN);

  // Release 'control' key.
  wl_keyboard_send_key(keyboard->resource(), ++serial, ++timestamp,
                       29 /* Control */, WL_KEYBOARD_KEY_STATE_RELEASED);
  Sync();

  wl_touch_send_down(touch_->resource(), ++serial, ++timestamp,
                     surface_->resource(), 0 /* id */, wl_fixed_from_int(50),
                     wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());
  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);

  wl_touch_send_motion(touch_->resource(), ++timestamp, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());
  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);

  wl_touch_send_up(touch_->resource(), ++serial, ++timestamp, 0 /* id */);
  wl_touch_send_frame(touch_->resource());
  Sync();
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get());
  EXPECT_FALSE(event->flags() & ui::EF_CONTROL_DOWN);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandTouchTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandTouchTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

}  // namespace ui
