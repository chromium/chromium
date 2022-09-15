// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_test.h"

#include <memory>

#include "base/run_loop.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

WaylandTest::WaylandTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                        base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto keyboard_layout_engine =
      std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_);
#else
  auto keyboard_layout_engine = std::make_unique<StubKeyboardLayoutEngine>();
#endif
  scoped_keyboard_layout_engine_ = std::make_unique<ScopedKeyboardLayoutEngine>(
      std::move(keyboard_layout_engine));
  connection_ = std::make_unique<WaylandConnection>();
  buffer_manager_gpu_ = std::make_unique<WaylandBufferManagerGpu>();
  surface_factory_ = std::make_unique<WaylandSurfaceFactory>(
      connection_.get(), buffer_manager_gpu_.get());
}

WaylandTest::~WaylandTest() {}

void WaylandTest::SetUp() {
  disabled_features_.push_back(ui::kWaylandSurfaceSubmissionInPixelCoordinates);
  disabled_features_.push_back(features::kWaylandScreenCoordinatesEnabled);

  feature_list_.InitWithFeatures(enabled_features_, disabled_features_);

  if (DeviceDataManager::HasInstance()) {
    // Another instance may have already been set before.
    DeviceDataManager::GetInstance()->ResetDeviceListsForTest();
  } else {
    DeviceDataManager::CreateInstance();
  }

  ASSERT_TRUE(server_.Start(GetParam()));
  ASSERT_TRUE(connection_->Initialize());
  screen_ = connection_->wayland_output_manager()->CreateWaylandScreen();
  connection_->wayland_output_manager()->InitWaylandScreen(screen_.get());
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&widget_));
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = PlatformWindowType::kWindow;
  window_ = delegate_.CreateWaylandWindow(connection_.get(),
                                          std::move(properties), true, true);
  ASSERT_NE(widget_, gfx::kNullAcceleratedWidget);

  window_->Show(false);

  // Wait for the client to flush all pending requests from initialization.
  base::RunLoop().RunUntilIdle();

  // Pause the server after it has responded to all incoming events.
  server_.Pause();

  auto id = window_->root_surface()->GetSurfaceId();
  surface_ = server_.GetObject<wl::MockSurface>(id);
  ASSERT_TRUE(surface_);

  // The surface must be activated before buffers are attached.
  ActivateSurface(server_.GetObject<wl::MockSurface>(id)->xdg_surface());

  Sync();

  EXPECT_EQ(0u,
            DeviceDataManager::GetInstance()->GetTouchscreenDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetKeyboardDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetMouseDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetTouchpadDevices().size());

  initialized_ = true;
}

void WaylandTest::TearDown() {
  if (initialized_)
    Sync();
}

void WaylandTest::Sync() {
  // Resume the server, flushing its pending events.
  server_.Resume();

  // Wait for the client to finish processing these events.
  base::RunLoop().RunUntilIdle();

  // Pause the server, after it has finished processing any follow-up requests
  // from the client.
  server_.Pause();
}

void WaylandTest::SetPointerFocusedWindow(WaylandWindow* window) {
  connection_->wayland_window_manager()->SetPointerFocusedWindow(window);
}

void WaylandTest::SetKeyboardFocusedWindow(WaylandWindow* window) {
  connection_->wayland_window_manager()->SetKeyboardFocusedWindow(window);
}

void WaylandTest::SendConfigureEvent(wl::MockXdgSurface* xdg_surface,
                                     const gfx::Size& size,
                                     uint32_t serial,
                                     struct wl_array* states) {
  const int32_t width = size.width();
  const int32_t height = size.height();
  // In xdg_shell_v6+, both surfaces send serial configure event and toplevel
  // surfaces send other data like states, heights and widths.
  // Please note that toplevel surfaces may not exist if the surface was created
  // for the popup role.
  if (GetParam().shell_version == wl::ShellVersion::kV6) {
    if (xdg_surface->xdg_toplevel()) {
      zxdg_toplevel_v6_send_configure(xdg_surface->xdg_toplevel()->resource(),
                                      width, height, states);
    } else {
      ASSERT_TRUE(xdg_surface->xdg_popup()->resource());
      zxdg_popup_v6_send_configure(xdg_surface->xdg_popup()->resource(), 0, 0,
                                   width, height);
    }
    zxdg_surface_v6_send_configure(xdg_surface->resource(), serial);
  } else {
    if (xdg_surface->xdg_toplevel()) {
      xdg_toplevel_send_configure(xdg_surface->xdg_toplevel()->resource(),
                                  width, height, states);
    } else {
      ASSERT_TRUE(xdg_surface->xdg_popup()->resource());
      xdg_popup_send_configure(xdg_surface->xdg_popup()->resource(), 0, 0,
                               width, height);
    }
    xdg_surface_send_configure(xdg_surface->resource(), serial);
  }
}

void WaylandTest::ActivateSurface(wl::MockXdgSurface* xdg_surface) {
  wl::ScopedWlArray state({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(xdg_surface, {0, 0}, 1, state.get());
}

void WaylandTest::InitializeSurfaceAugmenter() {
  server_.EnsureSurfaceAugmenter();
  Sync();
}

}  // namespace ui
