// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_seat.h"

#include <cursor-shape-v1-client-protocol.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_shape.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

using WaylandSeatTest = WaylandTestSimple;

// Tests that multiple send capability events result in a new device to be
// obtained on the client side to avoid a scenario where a stale device may be
// retained.
TEST_F(WaylandSeatTest, TestMultipleCapabilityEvents) {
  uint32_t prev_touch_id = 0, prev_keyboard_id = 0, prev_pointer_id = 0;
  for (size_t _ : {0, 1}) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_TOUCH |
                                    WL_SEAT_CAPABILITY_KEYBOARD |
                                    WL_SEAT_CAPABILITY_POINTER);
    });
    auto validate_new_device_obtained = [](auto* device, auto& prev_id) {
      ASSERT_TRUE(device);
      ASSERT_NE(device->id(), prev_id);
      prev_id = device->id();
    };

    validate_new_device_obtained(connection_->seat()->touch(), prev_touch_id);
    validate_new_device_obtained(connection_->seat()->keyboard(),
                                 prev_keyboard_id);
    validate_new_device_obtained(connection_->seat()->pointer(),
                                 prev_pointer_id);

    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetKeyboardDevices().size());
    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetTouchscreenDevices().size());
    EXPECT_EQ(1u, DeviceDataManager::GetInstance()->GetMouseDevices().size());
  }
}

class WaylandSeatCursorShapeTest : public WaylandTestSimple {
 public:
  // `supports_tablet` also advertises wp_cursor_shape_manager_v1.
  WaylandSeatCursorShapeTest() : WaylandTestSimple({.supports_tablet = true}) {}
};

// Tests that the cursor shape device is created for the current wl_pointer
// after the pointer capability is removed and added again, since the device of
// the old wl_pointer becomes inert.
TEST_F(WaylandSeatCursorShapeTest, NewShapeDeviceForNewPointer) {
  WaylandCursorShape* cursor_shape = connection_->wayland_cursor_shape();
  ASSERT_TRUE(cursor_shape);
  connection_->serial_tracker().UpdateSerial(wl::SerialType::kMouseEnter, 1);

  for (size_t _ : {0, 1}) {
    for (uint32_t capabilities : {0u, uint32_t{WL_SEAT_CAPABILITY_POINTER}}) {
      PostToServerAndWait([capabilities](wl::TestWaylandServerThread* server) {
        wl_seat_send_capabilities(server->seat()->resource(), capabilities);
      });
    }
    ASSERT_TRUE(connection_->seat()->pointer());

    cursor_shape->SetCursorShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT);
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(server->seat()->pointer());
      EXPECT_TRUE(server->seat()->pointer()->cursor_shape_device());
    });
  }
}

}  // namespace ui
