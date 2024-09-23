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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"
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

namespace ui {

class ScopedKeyboardLayoutEngine;
class WaylandScreen;

// WaylandTest is a base class that sets up a display, window, and test server,
// and allows easy synchronization between them.
class WaylandTestBase {
 public:
  explicit WaylandTestBase(wl::ServerConfig config);
  WaylandTestBase(const WaylandTestBase&) = delete;
  WaylandTestBase& operator=(const WaylandTestBase&) = delete;
  ~WaylandTestBase();

  void SetUp();
  void TearDown();

  // Posts 'callback' or 'closure' to run on the server thread; blocks till the
  // callable is run and all pending Wayland requests and events are delivered.
  // Note: This by default uses base::RunLoops which causes all posted tasks,
  // including tasks posted with a delay, to run during this call before all
  // server's events are processed by the client.
  // The 'no_nested_runloops' parameter can be used to not use runloops for
  // testing code that posts delayed tasks and the delay can be controlled in
  // the test without them being executed unexpectedly during this call.
  void PostToServerAndWait(
      base::OnceCallback<void(wl::TestWaylandServerThread* server)> callback,
      bool no_nested_runloops = true);
  void PostToServerAndWait(base::OnceClosure closure,
                           bool no_nested_runloops = true);

  // Similar to the two methods above, but provides the convenience of using a
  // capturing lambda directly.
  template <
      typename Lambda,
      typename = std::enable_if_t<
          std::is_invocable_r_v<void, Lambda, wl::TestWaylandServerThread*> ||
          std::is_invocable_r_v<void, Lambda>>>
  void PostToServerAndWait(Lambda&& lambda, bool no_nested_runloops = true) {
    PostToServerAndWait(base::BindLambdaForTesting(std::move(lambda)),
                        no_nested_runloops);
  }

  // Convenience wrapper function for WaylandConnectionTestApi::SyncDisplay.
  void SyncDisplay();

 protected:
  // Disables client-server sync during the teardown.  Used by tests that
  // intentionally spoil the client-server communication.
  void DisableSyncOnTearDown();

  void SetPointerFocusedWindow(WaylandWindow* window);
  void SetKeyboardFocusedWindow(WaylandWindow* window);

  // Sends configure event for the |surface_id|. Please note that |surface_id|
  // must be an id of the wl_surface that has xdg_surface role.
  void SendConfigureEvent(uint32_t surface_id,
                          const gfx::Size& size,
                          const wl::ScopedWlArray& states,
                          std::optional<uint32_t> serial = std::nullopt);

  // Sends XDG_TOPLEVEL_STATE_ACTIVATED to the surface that has |surface_id| and
  // has xdg surface role with width and height set to 0, which results in
  // asking the client to set the width and height of the surface. The client
  // test may pass |serial| that will be used to activate the surface.
  void ActivateSurface(uint32_t surface_id,
                       std::optional<uint32_t> serial = std::nullopt);

  // Initializes SurfaceAugmenter in |server_|.
  void InitializeSurfaceAugmenter();

  // A helper method that sets up the XKB configuration for tests that require
  // it.
  // Does nothing if XkbCommon is not used.
  void MaybeSetUpXkb();

  // A helper method to ensure that information for all displays are populated
  // and ready.
  void WaitForAllDisplaysReady();

  // Creates a Wayland window with the specified delegate, type, and bounds.
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithParams(
      PlatformWindowType type,
      const gfx::Rect bounds,
      MockWaylandPlatformWindowDelegate* delegate,
      gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget);

  base::test::TaskEnvironment task_environment_;

  wl::TestWaylandServerThread server_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  ::testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_;
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_keyboard_layout_engine_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<WaylandBufferManagerGpu> buffer_manager_gpu_;
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandScreen> screen_;
  std::unique_ptr<WaylandWindow> window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  std::vector<base::test::FeatureRef> enabled_features_{
      features::kLacrosColorManagement, ui::kWaylandOverlayDelegation};
  std::vector<base::test::FeatureRef> disabled_features_;

 private:
  bool initialized_ = false;
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  base::test::ScopedFeatureList feature_list_;
};

// Version of WaylandTestBase that uses parametrised tests (TEST_P).
class WaylandTest : public WaylandTestBase,
                    public ::testing::TestWithParam<wl::ServerConfig> {
 public:
  WaylandTest();
  WaylandTest(const WaylandTest&) = delete;
  WaylandTest& operator=(const WaylandTest&) = delete;
  ~WaylandTest() override;

  void SetUp() override;
  void TearDown() override;

  bool IsAuraShellEnabled();
};

// Version of WaylandTest that uses simple test fixtures (TEST_F).
class WaylandTestSimple : public WaylandTestBase, public ::testing::Test {
 public:
  WaylandTestSimple();
  explicit WaylandTestSimple(wl::ServerConfig);
  WaylandTestSimple(const WaylandTestSimple&) = delete;
  WaylandTestSimple& operator=(const WaylandTestSimple&) = delete;
  ~WaylandTestSimple() override;

  void SetUp() override;
  void TearDown() override;
};

// Version of WaylandTest that uses simple test fixtures (TEST_F) and
// aura_shell enabled.
class WaylandTestSimpleWithAuraShell : public WaylandTestBase,
                                       public ::testing::Test {
 public:
  WaylandTestSimpleWithAuraShell();
  WaylandTestSimpleWithAuraShell(const WaylandTestSimple&) = delete;
  WaylandTestSimpleWithAuraShell& operator=(const WaylandTestSimple&) = delete;
  ~WaylandTestSimpleWithAuraShell() override;

  void SetUp() override;
  void TearDown() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_
