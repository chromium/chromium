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

class XdgActivationDeferredHideTest : public XdgActivationTest {
 protected:
  void SetUp() override {
    XdgActivationTest::SetUp();
    window_.reset();
    delegate_ =
        std::make_unique<MockWaylandPlatformWindowDelegate>(connection_.get());
    requestor_ = CreateWaylandWindowWithParams(
        PlatformWindowType::kWindow, kDefaultBounds, delegate_.get(),
        gfx::kNullAcceleratedWidget, true /*inactive*/);
    target_ = CreateWaylandWindowWithParams(
        PlatformWindowType::kWindow, kDefaultBounds, delegate_.get(),
        gfx::kNullAcceleratedWidget, true /*inactive*/);
    requestor_id_ = requestor_->root_surface()->get_surface_id();
    target_id_ = target_->root_surface()->get_surface_id();
    ActivateSurface(requestor_id_);
    ActivateSurface(target_id_);
  }

  void TearDown() override {
    SetPointerFocusedWindow(nullptr);
    requestor_.reset();
    target_.reset();
    delegate_.reset();
    XdgActivationTest::TearDown();
  }

  // Emulates a click in `requestor_` that activates `target_`, which requests
  // a token on behalf of `requestor_`, and then hides `requestor_`.
  void ActivateTargetAndHideRequestor() {
    SetPointerFocusedWindow(requestor_.get());
    connection_->serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 42);
    PostToServerAndWait(
        [requestor_id = requestor_id_](wl::TestWaylandServerThread* server) {
          auto* const requestor_surface =
              server->GetObject<wl::MockSurface>(requestor_id)->resource();
          EXPECT_CALL(*server->xdg_activation_v1(),
                      TokenSetSurface(_, _, requestor_surface));
        });
    target_->Activate();
    requestor_->Hide();
    EXPECT_FALSE(requestor_->IsVisible());
  }

  bool IsRequestorMapped() {
    bool mapped = false;
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* const xdg_surface =
          server->GetObject<wl::MockSurface>(requestor_id_)->xdg_surface();
      mapped = xdg_surface && xdg_surface->xdg_toplevel();
    });
    return mapped;
  }

  std::unique_ptr<MockWaylandPlatformWindowDelegate> delegate_;
  std::unique_ptr<WaylandWindow> requestor_;
  std::unique_ptr<WaylandWindow> target_;
  uint32_t requestor_id_ = 0;
  uint32_t target_id_ = 0;
};

// Tests that a toplevel hidden while it is the requestor of an in-flight token
// request stays mapped until the token has been used to activate another
// toplevel.
TEST_F(XdgActivationDeferredHideTest, HideDeferredUntilTokenUsed) {
  ActivateTargetAndHideRequestor();
  EXPECT_TRUE(IsRequestorMapped());

  PostToServerAndWait([requestor_id = requestor_id_, target_id = target_id_](
                          wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const requestor_surface =
        server->GetObject<wl::MockSurface>(requestor_id);
    auto* const target_surface =
        server->GetObject<wl::MockSurface>(target_id)->resource();
    // The requestor must still be mapped when the token is used.
    EXPECT_CALL(*xdg_activation,
                Activate(_, _, StrEq(kMockStaticTestToken), target_surface))
        .WillOnce([requestor_surface]() {
          EXPECT_TRUE(requestor_surface->xdg_surface());
        });
    ASSERT_TRUE(xdg_activation->get_token());
    xdg_activation_token_v1_send_done(xdg_activation->get_token()->resource(),
                                      kMockStaticTestToken);
  });
  SyncDisplay();

  EXPECT_FALSE(IsRequestorMapped());
  EXPECT_FALSE(requestor_->IsVisible());
}

// Tests that a deferred hide also completes if the token request times out.
TEST_F(XdgActivationDeferredHideTest, HideDeferredUntilTimeout) {
  ActivateTargetAndHideRequestor();
  EXPECT_TRUE(IsRequestorMapped());

  task_environment_.FastForwardBy(base::Milliseconds(600));
  EXPECT_FALSE(IsRequestorMapped());
}

// Tests that showing the toplevel again finishes a deferred hide right away
// and maps the window again with a new xdg_toplevel, as configure events were
// ignored meanwhile.
TEST_F(XdgActivationDeferredHideTest, ShowFinishesDeferredHideAndRemaps) {
  ActivateTargetAndHideRequestor();
  ASSERT_TRUE(IsRequestorMapped());
  uint32_t old_toplevel_id = 0;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    old_toplevel_id =
        wl_resource_get_id(server->GetObject<wl::MockSurface>(requestor_id_)
                               ->xdg_surface()
                               ->xdg_toplevel()
                               ->resource());
  });

  requestor_->Show(/*inactive=*/true);
  EXPECT_TRUE(requestor_->IsVisible());
  ASSERT_TRUE(IsRequestorMapped());
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_NE(
        old_toplevel_id,
        wl_resource_get_id(server->GetObject<wl::MockSurface>(requestor_id_)
                               ->xdg_surface()
                               ->xdg_toplevel()
                               ->resource()));
  });

  // The completion of the token request doesn't hide the window anymore.
  task_environment_.FastForwardBy(base::Milliseconds(600));
  EXPECT_TRUE(IsRequestorMapped());
  EXPECT_TRUE(requestor_->IsVisible());
}

// Tests that configure, close and input events for a toplevel whose hide is
// deferred don't reach the delegate, which may already be tearing down (e.g. a
// closing widget).
TEST_F(XdgActivationDeferredHideTest, NoDelegateCallsWhileHideDeferred) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  ActivateTargetAndHideRequestor();
  ASSERT_TRUE(IsRequestorMapped());

  EXPECT_CALL(*delegate_, GetMinimumSizeForWindow()).Times(0);
  EXPECT_CALL(*delegate_, OnActivationChanged(_)).Times(0);
  EXPECT_CALL(*delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(*delegate_, OnCloseRequest()).Times(0);
  EXPECT_CALL(*delegate_, DispatchEvent(_)).Times(0);
  PostToServerAndWait([requestor_id =
                           requestor_id_](wl::TestWaylandServerThread* server) {
    auto* const xdg_surface =
        server->GetObject<wl::MockSurface>(requestor_id)->xdg_surface();
    ASSERT_TRUE(xdg_surface);
    EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
    xdg_toplevel_send_close(xdg_surface->xdg_toplevel()->resource());
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(10),
                           wl_fixed_from_double(20));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });
  ActivateSurface(requestor_id_);
  SyncDisplay();
  ::testing::Mock::VerifyAndClearExpectations(delegate_.get());
}

// Tests that destroying the toplevel while its hide is deferred is safe.
TEST_F(XdgActivationDeferredHideTest, DestroyWhileHideDeferred) {
  ActivateTargetAndHideRequestor();
  SetPointerFocusedWindow(nullptr);
  requestor_.reset();

  task_environment_.FastForwardBy(base::Milliseconds(600));
  SyncDisplay();
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
    if (!session_id_.has_value()) {
      auto* const session_manager = connection_->session_manager();
      CHECK(session_manager);
      session_id_ = session_manager->RestoreSession(
          wl::TestXxSessionManager::kTestSessionId,
          PlatformSessionManager::RestoreReason::kLaunch);
      CHECK(session_id_.has_value());
    }

    PlatformWindowInitProperties properties;
    properties.bounds = kDefaultBounds;
    properties.type = PlatformWindowType::kWindow;
    properties.session_id = session_id_.value();
    properties.session_window_new_id = next_window_id_++;
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

 private:
  std::optional<std::string> session_id_;
  int32_t next_window_id_ = 2;
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

// Tests that an unconfigured restored toplevel leaves the launch token alone so
// a newly opened session window can use it instead.
TEST_F(XdgActivationSessionTest, RestoredWindowLeavesLaunchTokenForNewWindow) {
  base::ScopedClosureRunner clear_token(
      base::BindOnce(base::IgnoreResult(&base::nix::TakeXdgActivationToken)));
  const std::string kLaunchToken = "LAUNCH_TOKEN";
  base::nix::SetActivationToken(kLaunchToken);

  MockWaylandPlatformWindowDelegate restored_delegate(connection_.get());
  auto restored_window =
      CreateSessionWindow(restored_delegate, /*restore_id=*/1);
  restored_window->Show(/*inactive=*/false);
  restored_window->Activate();
  EXPECT_FALSE(HasTokenRequest());

  MockWaylandPlatformWindowDelegate new_delegate(connection_.get());
  auto new_window =
      CreateSessionWindow(new_delegate, /*restore_id=*/std::nullopt);
  new_window->Show(/*inactive=*/false);
  EXPECT_FALSE(base::nix::TakeXdgActivationToken().has_value());
  EXPECT_FALSE(HasTokenRequest());

  const auto restored_surface_id =
      restored_window->root_surface()->get_surface_id();
  const auto new_surface_id = new_window->root_surface()->get_surface_id();
  PostToServerAndWait([restored_surface_id, new_surface_id,
                       kLaunchToken](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const restored_surface =
        server->GetObject<wl::MockSurface>(restored_surface_id)->resource();
    auto* const new_surface =
        server->GetObject<wl::MockSurface>(new_surface_id)->resource();
    EXPECT_CALL(*xdg_activation, Activate(_, _, _, restored_surface)).Times(0);
    EXPECT_CALL(*xdg_activation,
                Activate(_, _, StrEq(kLaunchToken), new_surface))
        .Times(1);
  });

  ActivateSurface(restored_surface_id);
  ActivateSurface(new_surface_id);
  SyncDisplay();
}

// Tests that once a restored toplevel is configured, runtime activation tokens
// (e.g. from external link opens or notifications) still activate it even
// before any user press inside the window.
TEST_F(XdgActivationSessionTest, ConfiguredRestoredWindowUsesRuntimeToken) {
  base::ScopedClosureRunner clear_token(
      base::BindOnce(base::IgnoreResult(&base::nix::TakeXdgActivationToken)));
  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateSessionWindow(delegate, /*restore_id=*/1);
  window->Show(/*inactive=*/false);

  const auto surface_id = window->root_surface()->get_surface_id();
  ActivateSurface(surface_id);
  SyncDisplay();

  const std::string kRuntimeToken = "RUNTIME_TOKEN";
  base::nix::SetActivationToken(kRuntimeToken);
  PostToServerAndWait([surface_id,
                       kRuntimeToken](wl::TestWaylandServerThread* server) {
    auto* const xdg_activation = server->xdg_activation_v1();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    EXPECT_CALL(*xdg_activation, Activate(_, _, StrEq(kRuntimeToken), surface))
        .Times(1);
  });

  window->Activate();
  EXPECT_FALSE(base::nix::TakeXdgActivationToken().has_value());
  SyncDisplay();
}

}  // namespace ui
