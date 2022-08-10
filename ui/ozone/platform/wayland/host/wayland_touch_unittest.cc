// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_touch_stylus.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
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

bool CompareFloat(float a, float b) {
  constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
  return std::isnan(a) ? std::isnan(b) : fabs(a - b) < kEpsilon;
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
      ui::EventPointerType pointer_type = ui::EventPointerType::kTouch,
      float force = std::numeric_limits<float>::quiet_NaN(),
      float tilt_x = 0.0,
      float tilt_y = 0.0) {
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());

    auto* touch_event = event->AsTouchEvent();
    EXPECT_EQ(event_type, touch_event->type());
    EXPECT_EQ(pointer_type, touch_event->pointer_details().pointer_type);
    EXPECT_TRUE(CompareFloat(force, touch_event->pointer_details().force));
    EXPECT_TRUE(CompareFloat(tilt_x, touch_event->pointer_details().tilt_x));
    EXPECT_TRUE(CompareFloat(tilt_y, touch_event->pointer_details().tilt_y));
  }

  raw_ptr<wl::TestTouch> touch_;
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
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get(),
                 ui::EventPointerType::kPen);
}

// Tests that touch events with stylus pen work. This variant of the test sends
// the tool information after the touch down event, and ensures that
// wl_touch::frame event handles it correctly.
TEST_P(WaylandTouchTest, TouchPressAndMotionWithStylus2) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  uint32_t time = 0;
  wl_touch_send_down(touch_->resource(), 1, 0, surface_->resource(), 0 /* id */,
                     wl_fixed_from_int(50), wl_fixed_from_int(100));
  zcr_touch_stylus_v2_send_tool(touch_->touch_stylus()->resource(), 0 /* id */,
                                ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN);
  zcr_touch_stylus_v2_send_force(touch_->touch_stylus()->resource(), ++time,
                                 0 /* id */, wl_fixed_from_double(1.0f));
  zcr_touch_stylus_v2_send_tilt(touch_->touch_stylus()->resource(), ++time,
                                0 /* id */, wl_fixed_from_double(-45),
                                wl_fixed_from_double(45));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get(), ui::EventPointerType::kPen,
                 1.0f /* force */, -45.0f /* tilt_x */, 45.0f /* tilt_y */);

  wl_touch_send_motion(touch_->resource(), 500, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get(), ui::EventPointerType::kPen,
                 1.0f /* force */, -45.0f /* tilt_x */, 45.0f /* tilt_y */);

  wl_touch_send_up(touch_->resource(), 1, 1000, 0 /* id */);
  wl_touch_send_frame(touch_->resource());

  Sync();
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get(), ui::EventPointerType::kPen,
                 1.0f /* force */, -45.0f /* tilt_x */, 45.0f /* tilt_y */);
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

#if BUILDFLAG(USE_XKBCOMMON)
  // Set up XKB bits and set the keymap to the client.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context(
      xkb_context_new(XKB_CONTEXT_NO_FLAGS));
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap(
      xkb_keymap_new_from_names(xkb_context.get(), nullptr /*names*/,
                                XKB_KEYMAP_COMPILE_NO_FLAGS));
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state(
      xkb_state_new(xkb_keymap.get()));

  std::unique_ptr<char, base::FreeDeleter> keymap_string(
      xkb_keymap_get_as_string(xkb_keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
  DCHECK(keymap_string.get());
  size_t keymap_size = strlen(keymap_string.get()) + 1;

  base::UnsafeSharedMemoryRegion shared_keymap_region =
      base::UnsafeSharedMemoryRegion::Create(keymap_size);
  base::WritableSharedMemoryMapping shared_keymap = shared_keymap_region.Map();
  base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_keymap_region));
  DCHECK(shared_keymap.IsValid());

  memcpy(shared_keymap.memory(), keymap_string.get(), keymap_size);
  wl_keyboard_send_keymap(
      keyboard->resource(), WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
      platform_shared_keymap.GetPlatformHandle().fd, keymap_size);
#endif

  // Press 'control' key.
  wl_keyboard_send_modifiers(keyboard->resource(), 3, 4 /* mods_depressed*/,
                             0 /* mods_latched */, 0 /* mods_locked */,
                             0 /* group */);
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
  wl_keyboard_send_modifiers(keyboard->resource(), 3, 0 /* mods_depressed*/,
                             0 /* mods_latched */, 0 /* mods_locked */,
                             0 /* group */);
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
