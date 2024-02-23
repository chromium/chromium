// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include "base/test/bind.h"
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

// Tests that XdgActivation uses the proper surface to request token.
TEST_P(XdgActivationTest, RequestNewToken) {
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

    // The following should be called once for the initial request and then
    // again when the queued request is sent after the initial one completes.
    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, surface2)).Times(2);
    EXPECT_CALL(*xdg_activation, TokenCommit(_, _)).Times(2);
  });

  bool first_request_completed = false;
  connection_->xdg_activation()->RequestNewToken(
      base::BindLambdaForTesting([&first_request_completed](std::string token) {
        EXPECT_EQ(token, kMockStaticTestToken);
        first_request_completed = true;
      }));
  connection_->xdg_activation()->RequestNewToken(
      base::BindLambdaForTesting([](std::string token) {
        FAIL() << "received second token even though the server only sent the "
                  "token once";
      }));
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const token = xdg_activation->get_token();

    xdg_activation_token_v1_send_done(token->resource(), kMockStaticTestToken);

    // Activate should NOT be called here
    EXPECT_CALL(*xdg_activation, Activate).Times(0);
  });

  EXPECT_TRUE(first_request_completed);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         XdgActivationTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
