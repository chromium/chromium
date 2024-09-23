// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::StrEq;
using ::testing::Values;

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);
const char kMockStaticTestToken[] = "CHROMIUM_MOCK_XDG_ACTIVATION_TOKEN";

}  // namespace

using XdgActivationTest = WaylandTestSimple;

// Tests that XdgActivation uses the proper surface to request token.
TEST_F(XdgActivationTest, RequestNewToken) {
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

    // The following should be called each time for the requests below,
    // including both successful cases and one timeout case.
    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, surface2)).Times(3);
    EXPECT_CALL(*xdg_activation, TokenCommit(_, _)).Times(3);
  });

  // Expect a successful token request.
  {
    ::testing::StrictMock<
        base::MockCallback<base::nix::XdgActivationTokenCallback>>
        callback;
    EXPECT_CALL(callback, Run(std::string(kMockStaticTestToken)));
    connection_->xdg_activation()->RequestNewToken(callback.Get());
    PostToServerAndWait(
        [](wl::TestWaylandServerThread* server) {
          auto* const xdg_activation = server->xdg_activation_v1();
          ASSERT_TRUE(xdg_activation);
          ASSERT_TRUE(xdg_activation->get_token());
          xdg_activation_token_v1_send_done(
              xdg_activation->get_token()->resource(), kMockStaticTestToken);
        },
        true);
  }

  // Expect a successful token request from a non-sequenced task.
  {
    ::testing::StrictMock<
        base::MockCallback<base::nix::XdgActivationTokenCallback>>
        callback;
    EXPECT_CALL(callback, Run(std::string(kMockStaticTestToken)));
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&XdgActivation::RequestNewToken,
                       base::Unretained(connection_->xdg_activation()),
                       callback.Get()));
    task_environment_.RunUntilIdle();
    PostToServerAndWait(
        [](wl::TestWaylandServerThread* server) {
          auto* const xdg_activation = server->xdg_activation_v1();
          ASSERT_TRUE(xdg_activation);
          ASSERT_TRUE(xdg_activation->get_token());
          xdg_activation_token_v1_send_done(
              xdg_activation->get_token()->resource(), kMockStaticTestToken);
        },
        true);
  }

  // Emulate a timeout.
  {
    ::testing::StrictMock<
        base::MockCallback<base::nix::XdgActivationTokenCallback>>
        callback;
    EXPECT_CALL(callback, Run(std::string()));
    connection_->xdg_activation()->RequestNewToken(callback.Get());
    task_environment_.FastForwardBy(base::Milliseconds(600));
  }
}

// Tests that with too many requests at some point the request queue will be
// full and the subsequent request callbacks will be run immediately with an
// empty token instead of adding them to the queue for sending to the server
// later.
TEST_F(XdgActivationTest, RequestNewToken_TooManyRequests) {
  // The first 100 requests should just be queued.
  for (int i = 0; i < 100; ++i) {
    connection_->xdg_activation()->RequestNewToken(
        base::BindOnce([](std::string _) {}));
  }
  // The next request should result in the callback being run immediately.
  ::testing::StrictMock<
      base::MockCallback<base::nix::XdgActivationTokenCallback>>
      callback;
  EXPECT_CALL(callback, Run(std::string()));
  connection_->xdg_activation()->RequestNewToken(callback.Get());
}

}  // namespace ui
