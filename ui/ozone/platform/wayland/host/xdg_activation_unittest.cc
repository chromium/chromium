// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include <linux/input.h>

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/nix/xdg_util.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_session_manager.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_xx_session_manager.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_session_manager.h"
#include "ui/platform_window/platform_window_init_properties.h"

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
  MockWaylandPlatformWindowDelegate delegate(connection_.get());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);
  });
  ASSERT_TRUE(connection_->seat()->keyboard());

  window_.reset();

  auto window1 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, kDefaultBounds, &delegate,
      gfx::kNullAcceleratedWidget, true /*inactive*/);
  auto window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, kDefaultBounds, &delegate,
      gfx::kNullAcceleratedWidget, true /*inactive*/);

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
    // again when the second request is sent after the initial one completes.
    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, surface2)).Times(2);
    EXPECT_CALL(*xdg_activation, TokenCommit(_, _)).Times(2);
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

// Tests that a token requested after a click in a bubble (a wl_subsurface,
// e.g. the profile menu) names the bubble's toplevel as the requestor surface
// rather than the bubble's own surface, which compositors don't accept.
TEST_F(XdgActivationTest, RequestNewTokenFromBubbleUsesToplevelSurface) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  MockWaylandPlatformWindowDelegate bubble_delegate(connection_.get());

  window_.reset();

  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, kDefaultBounds, &delegate,
      gfx::kNullAcceleratedWidget, true /*inactive*/);
  auto bubble = CreateWaylandWindowWithParams(
      PlatformWindowType::kBubble, gfx::Rect(10, 10, 50, 50), &bubble_delegate,
      toplevel->GetWidget(), true /*inactive*/);
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(bubble->AsWaylandBubble());
  ActivateSurface(toplevel->root_surface()->get_surface_id());

  // Emulate a mouse press on the bubble.
  SetPointerFocusedWindow(bubble.get());
  connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 42);

  PostToServerAndWait([toplevel_id = toplevel->root_surface()->get_surface_id(),
                       bubble_id = bubble->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const toplevel_surface =
        server->GetObject<wl::MockSurface>(toplevel_id)->resource();
    auto* const bubble_surface =
        server->GetObject<wl::MockSurface>(bubble_id)->resource();
    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, bubble_surface))
        .Times(0);
    EXPECT_CALL(*xdg_activation, TokenSetSurface(_, _, toplevel_surface));
    EXPECT_CALL(*xdg_activation, TokenCommit(_, _));
  });

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

  SetPointerFocusedWindow(nullptr);
}

// Tests that showing a new window automatically requests a token.
TEST_F(XdgActivationTest, RequestOnShow) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());

  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              kDefaultBounds, &delegate);
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

TEST_F(XdgActivationTest, MultipleActivateCallsNoClobber) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());

  // Create a window with inactive = true to avoid automatic activation on show.
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, kDefaultBounds, &delegate,
      gfx::kNullAcceleratedWidget, true /*inactive*/);

  // Set a launch token.
  const std::string kLaunchToken = "LAUNCH_TOKEN";
  base::nix::SetActivationToken(kLaunchToken);

  // First Activate call. It should consume the launch token and store it as pending.
  window->Activate();

  // Verify that the launch token was consumed from global state.
  EXPECT_FALSE(base::nix::TakeXdgActivationToken().has_value());

  // Second Activate call. It should NOT request a new token.
  window->Activate();

  // Verify on the server that no token request was received.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    ASSERT_TRUE(xdg_activation);
    EXPECT_FALSE(xdg_activation->get_token());
  });

  // Now we configure the window. The client should use the pending launch token.
  auto surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id, kLaunchToken](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    // Expect that the server receives the Activate request with the launch token.
    EXPECT_CALL(*xdg_activation, Activate(_, _, StrEq(kLaunchToken), surface)).Times(1);
  });

  // Trigger configure.
  ActivateSurface(surface_id);
  SyncDisplay();
}

TEST_F(XdgActivationTest, ActivateWithInitPropertiesToken) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());

  const std::string kStartupId = "PROPERTIES_TOKEN";
  const std::string kGlobalToken = "GLOBAL_TOKEN_SHOULD_NOT_BE_USED";
  base::nix::SetActivationToken(kGlobalToken);

  PlatformWindowInitProperties properties;
  properties.bounds = kDefaultBounds;
  properties.type = PlatformWindowType::kWindow;
  properties.startup_id = kStartupId;

  auto window = delegate.CreateWaylandWindow(connection_.get(), std::move(properties));
  window->Show(true /*inactive*/);

  // Verify that the global token was NOT consumed.
  auto global_token = base::nix::TakeXdgActivationToken();
  ASSERT_TRUE(global_token.has_value());
  EXPECT_EQ(global_token.value(), kGlobalToken);

  auto surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id, kStartupId](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    // Expect that the server receives the Activate request with the properties token.
    EXPECT_CALL(*xdg_activation, Activate(_, _, StrEq(kStartupId), surface)).Times(1);
  });

  // Trigger configure.
  ActivateSurface(surface_id);
  SyncDisplay();
}

class XdgActivationSessionTest : public WaylandTestSimple {
 public:
  XdgActivationSessionTest()
      : WaylandTestSimple({.supports_session_management = true}) {}

 protected:
  std::unique_ptr<WaylandWindow> CreateSessionWindow(
      MockWaylandPlatformWindowDelegate& delegate,
      std::optional<int32_t> restore_id) {
    auto* const session_manager = connection_->session_manager();
    CHECK(session_manager);
    auto session_id = session_manager->RestoreSession(
        wl::TestXxSessionManager::kTestSessionId,
        PlatformSessionManager::RestoreReason::kLaunch);
    CHECK(session_id.has_value());

    PlatformWindowInitProperties properties;
    properties.bounds = kDefaultBounds;
    properties.type = PlatformWindowType::kWindow;
    properties.session_id = session_id.value();
    properties.session_window_new_id = 2;
    properties.session_window_restore_id = restore_id;
    return delegate.CreateWaylandWindow(connection_.get(),
                                        std::move(properties));
  }

  bool HasTokenRequest() {
    bool has_token_request = false;
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      has_token_request = !!server->xdg_activation_v1()->get_token();
    });
    return has_token_request;
  }
};

// Tests that a toplevel restored from a session does not request an
// activation token until there is a user press event.
TEST_F(XdgActivationSessionTest, NoTokenRequestForRestoredWindow) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateSessionWindow(delegate, /*restore_id=*/1);
  window->Show(/*inactive=*/false);
  EXPECT_FALSE(HasTokenRequest());

  // SessionRestore activates the last active window after the windows are
  // mapped, which must not request one either, even if a restored window
  // mapped under the cursor and received wl_pointer.enter.
  auto surface_id = window->root_surface()->get_surface_id();
  ActivateSurface(surface_id);
  window->Activate();
  EXPECT_FALSE(HasTokenRequest());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
  });
  window->Activate();
  EXPECT_FALSE(HasTokenRequest());

  // Once there is a user press event, activation requests a token again.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });
  window->Activate();
  EXPECT_TRUE(HasTokenRequest());
}

// Tests that a new toplevel added to a session still requests a token.
TEST_F(XdgActivationSessionTest, TokenRequestForNewSessionWindow) {
  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateSessionWindow(delegate, /*restore_id=*/std::nullopt);
  ASSERT_FALSE(HasTokenRequest());
  window->Show(/*inactive=*/false);
  EXPECT_TRUE(HasTokenRequest());
}

// Tests that a restored toplevel still uses the launch token.
TEST_F(XdgActivationSessionTest, RestoredWindowUsesLaunchToken) {
  const std::string kLaunchToken = "LAUNCH_TOKEN";
  base::nix::SetActivationToken(kLaunchToken);

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateSessionWindow(delegate, /*restore_id=*/1);
  window->Show(/*inactive=*/false);
  EXPECT_FALSE(base::nix::TakeXdgActivationToken().has_value());
  EXPECT_FALSE(HasTokenRequest());

  auto surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id,
                       kLaunchToken](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    EXPECT_CALL(*xdg_activation, Activate(_, _, StrEq(kLaunchToken), surface))
        .Times(1);
  });

  ActivateSurface(surface_id);
  SyncDisplay();
}

}  // namespace ui
