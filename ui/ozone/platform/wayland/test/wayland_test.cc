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
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

WaylandTest::WaylandTest(TestServerMode server_mode)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                        base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      server_mode_(server_mode) {
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

  auto id = window_->root_surface()->get_surface_id();
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

  // TODO(crbug.com/1365887): this must be removed once all tests switch to
  // asynchronous mode.
  if (server_mode_ == TestServerMode::kAsync)
    server_.SetServerAsync();
}

void WaylandTest::TearDown() {
  if (initialized_) {
    if (server_mode_ != TestServerMode::kAsync)
      Sync();
    else
      SyncDisplay();
  }
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

void WaylandTest::PostToServerAndWait(
    base::OnceCallback<void(wl::TestWaylandServerThread* server)> callback) {
  // Sync with the display to ensure client's requests are processed.
  SyncDisplay();

  server_.RunAndWait(std::move(callback));

  // Sync with the display to ensure server's events are received and processed.
  SyncDisplay();
}

void WaylandTest::PostToServerAndWait(base::OnceClosure closure) {
  // Sync with the display to ensure client's requests are processed.
  SyncDisplay();

  server_.RunAndWait(std::move(closure));

  // Sync with the display to ensure server's events are received and processed
  SyncDisplay();
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
  // Please note that toplevel surfaces may not exist if the surface was created
  // for the popup role.
  if (xdg_surface->xdg_toplevel()) {
    xdg_toplevel_send_configure(xdg_surface->xdg_toplevel()->resource(), width,
                                height, states);
  } else {
    ASSERT_TRUE(xdg_surface->xdg_popup()->resource());
    xdg_popup_send_configure(xdg_surface->xdg_popup()->resource(), 0, 0, width,
                             height);
  }
  xdg_surface_send_configure(xdg_surface->resource(), serial);
}

void WaylandTest::SendConfigureEvent(uint32_t surface_id,
                                     const gfx::Size& size,
                                     const wl::ScopedWlArray& states,
                                     absl::optional<uint32_t> serial) {
  ASSERT_EQ(server_mode_, TestServerMode::kAsync);
  PostToServerAndWait([size, surface_id, states,
                       serial](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(surface);
    auto* xdg_surface = surface->xdg_surface();
    ASSERT_TRUE(xdg_surface);

    const int32_t width = size.width();
    const int32_t height = size.height();
    // In xdg_shell_v6+, both surfaces send serial configure event and toplevel
    // surfaces send other data like states, heights and widths.
    // Please note that toplevel surfaces may not exist if the surface was
    // created for the popup role.
    wl::ScopedWlArray surface_states(states);
    if (xdg_surface->xdg_toplevel()) {
      xdg_toplevel_send_configure(xdg_surface->xdg_toplevel()->resource(),
                                  width, height, surface_states.get());
    } else {
      ASSERT_TRUE(xdg_surface->xdg_popup()->resource());
      xdg_popup_send_configure(xdg_surface->xdg_popup()->resource(), 0, 0,
                               width, height);
    }
    xdg_surface_send_configure(
        xdg_surface->resource(),
        serial.has_value() ? serial.value() : server->GetNextSerial());
  });
}

void WaylandTest::ActivateSurface(wl::MockXdgSurface* xdg_surface) {
  wl::ScopedWlArray state({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(xdg_surface, {0, 0}, 1, state.get());
}

void WaylandTest::InitializeSurfaceAugmenter() {
  if (server_mode_ == TestServerMode::kAsync) {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      server->EnsureSurfaceAugmenter();
    });
  } else {
    server_.EnsureSurfaceAugmenter();
    Sync();
  }
}

void WaylandTest::SyncDisplay() {
  ASSERT_EQ(server_mode_, TestServerMode::kAsync);
  DCHECK(initialized_);
  base::RunLoop run_loop;
  wl::Object<wl_callback> sync_callback(
      wl_display_sync(connection_->display_wrapper()));
  wl_callback_listener listener = {
      [](void* data, struct wl_callback* cb, uint32_t time) {
        static_cast<base::RunLoop*>(data)->Quit();
      }};
  wl_callback_add_listener(sync_callback.get(), &listener, &run_loop);
  connection_->Flush();
  run_loop.Run();
}

}  // namespace ui
