// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#endif

namespace wl {
class MockSurface;
class MockXdgSurface;
}  // namespace wl

namespace ui {

class ScopedKeyboardLayoutEngine;
class WaylandScreen;

// WaylandTest is a base class that sets up a display, window, and test server,
// and allows easy synchronization between them.
class WaylandTest : public ::testing::TestWithParam<wl::ServerConfig> {
 public:
  // Specifies how the server should run.
  // TODO(crbug.com/1365887): this must be removed once all tests switch to
  // asynchronous mode.
  enum class TestServerMode {
    // The server will not be paused. The tests are expected to use
    // PostToServerAndWait to access libwayland-server APIs.
    kAsync = 0,
    // The server will be paused. The tests directly access libwayland-server
    // APIs.
    kSync
  };

  explicit WaylandTest(TestServerMode server_mode = TestServerMode::kSync);

  WaylandTest(const WaylandTest&) = delete;
  WaylandTest& operator=(const WaylandTest&) = delete;

  ~WaylandTest() override;

  void SetUp() override;
  void TearDown() override;

  void Sync();

  // Posts 'callback' or 'closure' to run on the client thread; blocks till the
  // callable is run and all pending Wayland requests and events are delivered.
  void PostToServerAndWait(
      base::OnceCallback<void(wl::TestWaylandServerThread* server)> callback);
  void PostToServerAndWait(base::OnceClosure closure);

  // Similar to the two methods above, but provides the convenience of using a
  // capturing lambda directly.
  template <
      typename Lambda,
      typename = std::enable_if_t<
          std::is_invocable_r_v<void, Lambda, wl::TestWaylandServerThread*> ||
          std::is_invocable_r_v<void, Lambda>>>
  void PostToServerAndWait(Lambda&& lambda) {
    PostToServerAndWait(base::BindLambdaForTesting(std::move(lambda)));
  }

 protected:
  void SetPointerFocusedWindow(WaylandWindow* window);
  void SetKeyboardFocusedWindow(WaylandWindow* window);

  // Sends configure event for the |xdg_surface|.
  void SendConfigureEvent(wl::MockXdgSurface* xdg_surface,
                          const gfx::Size& size,
                          uint32_t serial,
                          struct wl_array* states);
  // Sends configure event for the |surface_id|. Please note that |surface_id|
  // must be an id of the wl_surface that has xdg_surface role.
  void SendConfigureEvent(uint32_t surface_id,
                          const gfx::Size& size,
                          const wl::ScopedWlArray& states,
                          absl::optional<uint32_t> serial = absl::nullopt);

  // Sends XDG_TOPLEVEL_STATE_ACTIVATED to the |xdg_surface| with width and
  // height set to 0, which results in asking the client to set the width and
  // height of the surface.
  void ActivateSurface(wl::MockXdgSurface* xdg_surface);

  // Initializes SurfaceAugmenter in |server_|.
  void InitializeSurfaceAugmenter();

  // Sets up a sync callback via wl_display.sync and waits until it's received.
  // Requests are handled in-order and events are delivered in-order, thus sync
  // is used as a barrier to ensure all previous requests and the resulting
  // events have been handled.
  void SyncDisplay();

  base::test::TaskEnvironment task_environment_;

  wl::TestWaylandServerThread server_;
  raw_ptr<wl::MockSurface> surface_;

  MockWaylandPlatformWindowDelegate delegate_;
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_keyboard_layout_engine_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<WaylandBufferManagerGpu> buffer_manager_gpu_;
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandScreen> screen_;
  std::unique_ptr<WaylandWindow> window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  std::vector<base::test::FeatureRef> enabled_features_{
      ui::kWaylandOverlayDelegation};
  std::vector<base::test::FeatureRef> disabled_features_;

 private:
  bool initialized_ = false;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  base::test::ScopedFeatureList feature_list_;

  // The server will be set to asynchronous mode once started.
  const TestServerMode server_mode_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_
