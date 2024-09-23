// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {

using ::testing::Values;

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);

}  // namespace

class WaylandWindowManagerTest : public WaylandTest {
 public:
  WaylandWindowManagerTest() = default;
  WaylandWindowManagerTest(const WaylandWindowManagerTest&) = delete;
  WaylandWindowManagerTest& operator=(const WaylandWindowManagerTest&) = delete;
  ~WaylandWindowManagerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    manager_ = connection_->window_manager();
    ASSERT_TRUE(manager_);
  }

 protected:
  raw_ptr<WaylandWindowManager> manager_ = nullptr;
};

TEST_P(WaylandWindowManagerTest, GetWindow) {
  MockWaylandPlatformWindowDelegate delegate;

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  gfx::AcceleratedWidget window1_widget = window1->GetWidget();

  auto window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 300, 300), &delegate);

  EXPECT_TRUE(window1.get() == manager_->GetWindow(window1->GetWidget()));
  EXPECT_TRUE(window2.get() == manager_->GetWindow(window2->GetWidget()));

  window1.reset();

  EXPECT_FALSE(manager_->GetWindow(window1_widget));
}

TEST_P(WaylandWindowManagerTest, GetWindowWithLargestBounds) {
  MockWaylandPlatformWindowDelegate delegate;

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // There is also default window create by the WaylandTest. Thus, make bounds
  // of this window large enough.
  auto window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 3000, 3000), &delegate);

  EXPECT_TRUE(window2.get() == manager_->GetWindowWithLargestBounds());
}

TEST_P(WaylandWindowManagerTest, GetCurrentFocusedWindow) {
  MockWaylandPlatformWindowDelegate delegate;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // When window is shown, it automatically gets keyboard focus. Reset it.
  connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);

  WaylandTestBase::SyncDisplay();

  EXPECT_FALSE(manager_->GetCurrentFocusedWindow());
  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());
  EXPECT_FALSE(manager_->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_FALSE(manager_->GetCurrentPointerFocusedWindow());

  PostToServerAndWait([surface_id = window1->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());
  EXPECT_EQ(window1.get(), manager_->GetCurrentFocusedWindow());
  EXPECT_EQ(window1.get(), manager_->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window1.get(), manager_->GetCurrentPointerFocusedWindow());

  PostToServerAndWait([surface_id = window1->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_FALSE(manager_->GetCurrentFocusedWindow());
  EXPECT_FALSE(manager_->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_FALSE(manager_->GetCurrentPointerFocusedWindow());
}

TEST_P(WaylandWindowManagerTest, GetCurrentKeyboardFocusedWindow) {
  MockWaylandPlatformWindowDelegate delegate;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);
  });
  ASSERT_TRUE(connection_->seat()->keyboard());

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // When window is shown, it automatically gets keyboard focus. Reset it.
  connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);

  WaylandTestBase::SyncDisplay();

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());

  PostToServerAndWait([surface_id = window1->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl::ScopedWlArray empty({});
    wl_keyboard_send_enter(keyboard, server->GetNextSerial(), surface,
                           empty.get());
  });

  EXPECT_FALSE(manager_->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window1.get(), manager_->GetCurrentFocusedWindow());
  EXPECT_EQ(window1.get(), manager_->GetCurrentKeyboardFocusedWindow());

  PostToServerAndWait([surface_id = window1->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_keyboard_send_leave(keyboard, server->GetNextSerial(), surface);
  });

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());
}

TEST_P(WaylandWindowManagerTest, GetAllWindows) {
  MockWaylandPlatformWindowDelegate delegate;

  // There is a default window created by WaylandTest.
  auto windows = manager_->GetAllWindows();
  EXPECT_EQ(1u, windows.size());

  window_.reset();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  auto window2 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  windows = manager_->GetAllWindows();
  EXPECT_EQ(2u, windows.size());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowManagerTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
