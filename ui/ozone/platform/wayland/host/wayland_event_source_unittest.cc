// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::Values;

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);

}  // namespace

class WaylandEventSourceTest : public WaylandTest {
 public:
  WaylandEventSourceTest() {}

  WaylandEventSourceTest(const WaylandEventSourceTest&) = delete;
  WaylandEventSourceTest& operator=(const WaylandEventSourceTest&) = delete;

  void SetUp() override {
    WaylandTest::SetUp();

    pointer_delegate_ = connection_->event_source();
    DCHECK(pointer_delegate_);
  }

 protected:
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithParams(
      PlatformWindowType type,
      const gfx::Rect bounds,
      MockPlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = type;
    auto window = WaylandWindow::Create(delegate, connection_.get(),
                                        std::move(properties));
    if (window)
      window->Show(false);
    return window;
  }

  raw_ptr<WaylandPointer::Delegate> pointer_delegate_ = nullptr;
};

// Verify WaylandEventSource properly manages its internal state as pointer
// button events are sent. More specifically - pointer flags.
TEST_P(WaylandEventSourceTest, CheckPointerButtonHandling) {
  MockPlatformWindowDelegate delegate;
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_BACK_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_FORWARD_MOUSE_BUTTON));

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  Sync();

  ASSERT_TRUE(server_.seat()->pointer());

  uint32_t serial = 0;
  uint32_t tstamp = 0;
  wl_resource* surface_res =
      server_
          .GetObject<wl::MockSurface>(window1->root_surface()->GetSurfaceId())
          ->resource();
  wl_resource* pointer_res = server_.seat()->pointer()->resource();

  wl_pointer_send_enter(pointer_res, serial++, surface_res, 0, 0);
  wl_pointer_send_frame(pointer_res);
  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  Sync();

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));

  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(1);
  Sync();

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_RELEASED);
  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_RELEASED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  Sync();

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
}

// Verify WaylandEventSource properly manages its internal state as pointer
// button events are sent. More specifically - pointer flags.
TEST_P(WaylandEventSourceTest, DeleteBeforeTouchFrame) {
  MockPlatformWindowDelegate delegate;
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_TOUCH);

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  Sync();

  ASSERT_TRUE(server_.seat()->touch());

  uint32_t serial = 0;
  uint32_t tstamp = 0;
  wl_resource* surface_res =
      server_
          .GetObject<wl::MockSurface>(window1->root_surface()->GetSurfaceId())
          ->resource();
  wl_resource* touch_res = server_.seat()->touch()->resource();

  wl_touch_send_down(touch_res, serial++, tstamp++, surface_res, /*id=*/0, 0,
                     0);
  wl_touch_send_down(touch_res, serial++, tstamp++, surface_res, /*id=*/1, 0,
                     0);

  Sync();

  // Removint the target during touch event sequece should not cause crash.
  window1.reset();

  wl_touch_send_frame(touch_res);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);

  Sync();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandEventSourceTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandEventSourceTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

}  // namespace ui
