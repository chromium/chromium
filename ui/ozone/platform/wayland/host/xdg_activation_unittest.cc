// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_util.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::StrEq;
using ::testing::Values;

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);
const char kMockStaticTestToken[] = "CHROMIUM_MOCK_XDG_ACTIVATION_TOKEN";

}  // namespace

using XdgActivationTest = WaylandTest;

// Tests that XdgActivation uses the proper surface to request window
// activation.
TEST_P(XdgActivationTest, WindowActivation) {
  MockWaylandPlatformWindowDelegate delegate;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);
  });
  ASSERT_TRUE(connection_->seat()->keyboard());

  window_.reset();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  auto window2 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  // When window is shown, it automatically gets keyboard focus. Reset it
  connection_->window_manager()->SetKeyboardFocusedWindow(nullptr);

  auto surface_id1 = window1->root_surface()->get_surface_id();
  auto surface_id2 = window2->root_surface()->get_surface_id();

  ActivateSurface(surface_id1);
  ActivateSurface(surface_id2);

  PostToServerAndWait([surface_id2](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const surface2 =
        server->GetObject<wl::MockSurface>(surface_id2)->resource();

    wl::ScopedWlArray empty({});
    wl_keyboard_send_enter(keyboard, server->GetNextSerial(), surface2,
                           empty.get());

    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, surface2));
    EXPECT_CALL(*xdg_activation, TokenCommit(_, _));
  });

  connection_->xdg_activation()->Activate(window1->root_surface()->surface());

  PostToServerAndWait([surface_id1](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const token = xdg_activation->get_token();

    auto* const surface1 =
        server->GetObject<wl::MockSurface>(surface_id1)->resource();

    xdg_activation_token_v1_send_done(token->resource(), kMockStaticTestToken);

    EXPECT_CALL(*xdg_activation,
                Activate(_, xdg_activation->resource(),
                         StrEq(kMockStaticTestToken), surface1));
  });
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         XdgActivationTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
