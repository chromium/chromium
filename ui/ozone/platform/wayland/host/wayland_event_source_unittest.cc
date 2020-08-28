// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);

}  // namespace

class WaylandEventSourceTest : public WaylandTest {
 public:
  WaylandEventSourceTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    event_source_ = connection_->event_source();
    DCHECK(event_source_);
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

  WaylandEventSource* event_source_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandEventSourceTest);
};

// Verify WaylandEventSource properly manages its internal state as pointer
// button events are sent. More specifically - pointer flags.
TEST_P(WaylandEventSourceTest, CheckPointerButtonHandling) {
  MockPlatformWindowDelegate delegate;
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();

  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_BACK_MOUSE_BUTTON));
  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_FORWARD_MOUSE_BUTTON));

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
  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  Sync();

  EXPECT_TRUE(event_source_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));

  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(1);
  Sync();

  EXPECT_TRUE(event_source_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_RELEASED);
  wl_pointer_send_button(pointer_res, serial++, tstamp++, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_RELEASED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  Sync();

  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(event_source_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandEventSourceTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandEventSourceTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
