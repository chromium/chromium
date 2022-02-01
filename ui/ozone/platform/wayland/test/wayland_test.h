// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_

#include <memory>

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
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

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
  WaylandTest();

  WaylandTest(const WaylandTest&) = delete;
  WaylandTest& operator=(const WaylandTest&) = delete;

  ~WaylandTest() override;

  void SetUp() override;
  void TearDown() override;

  void Sync();

 protected:
  // Sends configure event for the |xdg_surface|.
  void SendConfigureEvent(wl::MockXdgSurface* xdg_surface,
                          int width,
                          int height,
                          uint32_t serial,
                          struct wl_array* states);

  // Sends XDG_TOPLEVEL_STATE_ACTIVATED to the |xdg_surface| with width and
  // height set to 0, which results in asking the client to set the width and
  // height of the surface.
  void ActivateSurface(wl::MockXdgSurface* xdg_surface);

  // Initializes SurfaceAugmenter in |server_|.
  void InitializeSurfaceAugmenter();

  base::test::TaskEnvironment task_environment_;

  wl::TestWaylandServerThread server_;
  wl::MockSurface* surface_;

  MockPlatformWindowDelegate delegate_;
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_keyboard_layout_engine_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<WaylandBufferManagerGpu> buffer_manager_gpu_;
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandScreen> screen_;
  std::unique_ptr<WaylandWindow> window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  std::vector<base::Feature> enabled_features_{ui::kWaylandOverlayDelegation};
  std::vector<base::Feature> disabled_features_;

 private:
  bool initialized_ = false;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_TEST_H_
