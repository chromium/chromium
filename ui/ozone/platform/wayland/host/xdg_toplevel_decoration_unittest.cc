// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

namespace {

constexpr uint32_t kClientSide = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
constexpr uint32_t kServerSide = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;

class XdgToplevelDecorationTest : public WaylandTestSimple {
 public:
  XdgToplevelDecorationTest()
      : WaylandTestSimple({.supports_xdg_decoration = true}) {}

 protected:
  std::vector<uint32_t> GetRequestedModes() {
    std::vector<uint32_t> modes;
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      modes = server->zxdg_decoration_manager_v1()->requested_modes();
    });
    return modes;
  }

  // Simulates an up to date frame from viz being latched.
  void AdvanceFrameToCurrent(
      WaylandWindow* window,
      const MockWaylandPlatformWindowDelegate& delegate) {
    SyncDisplay();
    window->OnSequencePoint(delegate.viz_seq());
    window->root_surface()->ApplyPendingState();
    window->OnFrameCommitted(delegate.viz_seq());
  }
};

}  // namespace

TEST_F(XdgToplevelDecorationTest, RequestsModeOnFrameTypeChange) {
  EXPECT_EQ(GetRequestedModes(), std::vector<uint32_t>({kClientSide}));

  window_->SetUseNativeFrame(true);
  EXPECT_EQ(GetRequestedModes(),
            std::vector<uint32_t>({kClientSide, kServerSide}));

  window_->SetUseNativeFrame(false);
  EXPECT_EQ(GetRequestedModes(),
            std::vector<uint32_t>({kClientSide, kServerSide, kClientSide}));
}

// Compositors like Sway force server-side decorations on tiled windows. Even
// though the compositor already uses the requested mode, the request must be
// sent so that the compositor responds with a configure that updates the
// window bounds for the new frame insets (crbug.com/378939144).
TEST_F(XdgToplevelDecorationTest, RequestsModeAlreadyForcedByCompositor) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zxdg_decoration_manager_v1()->set_forced_mode(kServerSide);
  });
  // Create a new window so the initial request gets the forced mode.
  ::testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate(
      connection_.get());
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 800, 600), &delegate);
  ASSERT_TRUE(window);
  EXPECT_EQ(GetRequestedModes(),
            std::vector<uint32_t>({kClientSide, kClientSide}));

  window->SetUseNativeFrame(true);
  EXPECT_EQ(GetRequestedModes(),
            std::vector<uint32_t>({kClientSide, kClientSide, kServerSide}));
}

// The compositor answers set_mode with an xdg_surface.configure, which must
// drop the client-side shadow insets from the bounds and window geometry when
// the window switches to the native frame, even if the compositor already
// forced server-side decorations (crbug.com/378939144). The configure either
// keeps the window geometry size (like Sway) or is 0x0, letting the client
// pick the size.
class XdgToplevelDecorationShadowTest
    : public XdgToplevelDecorationTest,
      public ::testing::WithParamInterface<gfx::Size> {};

TEST_P(XdgToplevelDecorationShadowTest, NativeFrameDropsShadowInsets) {
  constexpr gfx::Insets kShadowInsets(10);
  constexpr gfx::Size kGeometrySize(800, 600);
  const gfx::Size configure_size = GetParam();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->zxdg_decoration_manager_v1()->set_forced_mode(kServerSide);
  });
  ::testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate(
      connection_.get());
  gfx::Insets insets = kShadowInsets;
  ON_CALL(delegate, CalculateInsetsInDIP(::testing::_))
      .WillByDefault(::testing::ReturnPointee(&insets));
  const gfx::Size initial_size(kGeometrySize.width() + kShadowInsets.width(),
                               kGeometrySize.height() + kShadowInsets.height());
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(initial_size), &delegate);
  ASSERT_TRUE(window);
  const uint32_t surface_id = window->root_surface()->get_surface_id();

  SendConfigureEvent(surface_id, kGeometrySize,
                     wl::ScopedWlArray({XDG_TOPLEVEL_STATE_ACTIVATED}));
  AdvanceFrameToCurrent(window.get(), delegate);
  EXPECT_EQ(window->GetBoundsInDIP().size(), initial_size);

  // The native frame has no shadow.
  insets = gfx::Insets();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->zxdg_decoration_manager_v1()->set_toplevel_configure_size(
        configure_size);
    auto* xdg_surface =
        server->GetObject<wl::MockSurface>(surface_id)->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(::testing::_))
        .Times(::testing::AnyNumber());
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(kGeometrySize)));
  });
  window->SetUseNativeFrame(true);
  AdvanceFrameToCurrent(window.get(), delegate);
  EXPECT_EQ(window->GetBoundsInDIP().size(), kGeometrySize);

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* xdg_surface =
        server->GetObject<wl::MockSurface>(surface_id)->xdg_surface();
    ::testing::Mock::VerifyAndClearExpectations(xdg_surface);
  });
}

INSTANTIATE_TEST_SUITE_P(All,
                         XdgToplevelDecorationShadowTest,
                         ::testing::Values(gfx::Size(800, 600), gfx::Size()));

}  // namespace ui
