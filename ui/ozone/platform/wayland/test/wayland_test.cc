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
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_connection_test_api.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

WaylandTestBase::WaylandTestBase(wl::ServerConfig config)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                        base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      server_(config) {
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

WaylandTestBase::~WaylandTestBase() = default;

void WaylandTestBase::SetUp() {
  disabled_features_.push_back(ui::kWaylandSurfaceSubmissionInPixelCoordinates);

  feature_list_.InitWithFeatures(enabled_features_, disabled_features_);

  if (DeviceDataManager::HasInstance()) {
    // Another instance may have already been set before.
    DeviceDataManager::GetInstance()->ResetDeviceListsForTest();
  } else {
    DeviceDataManager::CreateInstance();
  }

  ASSERT_TRUE(server_.Start());
  ASSERT_TRUE(connection_->Initialize());
  screen_ = connection_->wayland_output_manager()->CreateWaylandScreen();
  connection_->wayland_output_manager()->InitWaylandScreen(screen_.get());
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&widget_));
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = PlatformWindowType::kWindow;
  window_ =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(widget_, gfx::kNullAcceleratedWidget);

  window_->Show(false);

  // Wait for the client to flush all pending requests from initialization.
  SyncDisplay();

  // The surface must be activated before buffers are attached.
  ActivateSurface(window_->root_surface()->get_surface_id());

  EXPECT_EQ(0u,
            DeviceDataManager::GetInstance()->GetTouchscreenDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetKeyboardDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetMouseDevices().size());
  EXPECT_EQ(0u, DeviceDataManager::GetInstance()->GetTouchpadDevices().size());

  initialized_ = true;
}

void WaylandTestBase::TearDown() {
  if (initialized_) {
    SyncDisplay();
  }
}

void WaylandTestBase::PostToServerAndWait(
    base::OnceCallback<void(wl::TestWaylandServerThread* server)> callback,
    bool no_nested_runloops) {
  PostToServerAndWait(
      base::BindOnce(std::move(callback), base::Unretained(&server_)),
      no_nested_runloops);
}

void WaylandTestBase::PostToServerAndWait(base::OnceClosure closure,
                                          bool no_nested_runloops) {
  if (no_nested_runloops) {
    // Ensure server processes pending requests.
    connection_->RoundTripQueue();

    // Post the closure to the server's thread.
    server_.Post(std::move(closure));
    // Wait for server thread to complete running posted tasks.
    server_.FlushForTesting();

    // Flush all non-delayed tasks.
    task_environment_.RunUntilIdle();
  } else {
    // Sync with the display to ensure client's requests are processed.
    SyncDisplay();

    server_.RunAndWait(std::move(closure));

    // Sync with the display to ensure server's events are received and
    // processed
    SyncDisplay();
  }
}

void WaylandTestBase::DisableSyncOnTearDown() {
  initialized_ = false;
}

void WaylandTestBase::SetPointerFocusedWindow(WaylandWindow* window) {
  connection_->window_manager()->SetPointerFocusedWindow(window);
}

void WaylandTestBase::SetKeyboardFocusedWindow(WaylandWindow* window) {
  connection_->window_manager()->SetKeyboardFocusedWindow(window);
}

void WaylandTestBase::SendConfigureEvent(uint32_t surface_id,
                                         const gfx::Size& size,
                                         const wl::ScopedWlArray& states,
                                         std::optional<uint32_t> serial) {
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

void WaylandTestBase::ActivateSurface(uint32_t surface_id,
                                      std::optional<uint32_t> serial) {
  wl::ScopedWlArray state({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(surface_id, {0, 0}, state, serial);
}

void WaylandTestBase::InitializeSurfaceAugmenter() {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->EnsureSurfaceAugmenter();
  });
}

void WaylandTestBase::MaybeSetUpXkb() {
#if BUILDFLAG(USE_XKBCOMMON)
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Set up XKB bits and set the keymap to the client.
    std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context(
        xkb_context_new(XKB_CONTEXT_NO_FLAGS));
    std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap(
        xkb_keymap_new_from_names(xkb_context.get(), nullptr /*names*/,
                                  XKB_KEYMAP_COMPILE_NO_FLAGS));
    std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state(
        xkb_state_new(xkb_keymap.get()));

    std::unique_ptr<char, base::FreeDeleter> keymap_string(
        xkb_keymap_get_as_string(xkb_keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    ASSERT_TRUE(keymap_string.get());

    size_t keymap_size = strlen(keymap_string.get()) + 1;
    base::UnsafeSharedMemoryRegion shared_keymap_region =
        base::UnsafeSharedMemoryRegion::Create(keymap_size);
    base::WritableSharedMemoryMapping shared_keymap =
        shared_keymap_region.Map();
    base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shared_keymap_region));
    ASSERT_TRUE(shared_keymap.IsValid());

    memcpy(shared_keymap.memory(), keymap_string.get(), keymap_size);

    auto* const keyboard = server->seat()->keyboard()->resource();
    ASSERT_TRUE(keyboard);

    wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            platform_shared_keymap.GetPlatformHandle().fd,
                            keymap_size);
  });
#endif
}

void WaylandTestBase::WaitForAllDisplaysReady() {
  // First, make sure all outputs are created and are ready.
  base::RunLoop loop;
  base::RepeatingTimer timer;
  timer.Start(
      FROM_HERE, base::Milliseconds(1), base::BindLambdaForTesting([&]() {
        auto& outputs = connection_->wayland_output_manager()->GetAllOutputs();
        for (auto& output : outputs) {
          // Displays are updated when the output is ready.
          if (!output.second->IsReady())
            return;
        }
        return loop.Quit();
      }));
  loop.Run();

  // Secondly, make sure all events after 'done' are processed.
  SyncDisplay();
}

std::unique_ptr<WaylandWindow> WaylandTestBase::CreateWaylandWindowWithParams(
    PlatformWindowType type,
    const gfx::Rect bounds,
    MockWaylandPlatformWindowDelegate* delegate,
    gfx::AcceleratedWidget parent_widget) {
  PlatformWindowInitProperties properties;
  properties.bounds = bounds;
  properties.type = type;
  properties.parent_widget = parent_widget;

  auto window =
      delegate->CreateWaylandWindow(connection_.get(), std::move(properties));
  if (window)
    window->Show(false);
  return window;
}

void WaylandTestBase::SyncDisplay() {
  WaylandConnectionTestApi(connection_.get()).SyncDisplay();
}

WaylandTest::WaylandTest() : WaylandTestBase(GetParam()) {}

WaylandTest::~WaylandTest() = default;

void WaylandTest::SetUp() {
  WaylandTestBase::SetUp();
}

void WaylandTest::TearDown() {
  WaylandTestBase::TearDown();
}

bool WaylandTest::IsAuraShellEnabled() {
  return GetParam().enable_aura_shell == wl::EnableAuraShellProtocol::kEnabled;
}

WaylandTestSimple::WaylandTestSimple()
    : WaylandTestSimple(wl::ServerConfig{}) {}

WaylandTestSimple::WaylandTestSimple(wl::ServerConfig config)
    : WaylandTestBase(config) {}

WaylandTestSimple::~WaylandTestSimple() = default;

void WaylandTestSimple::SetUp() {
  WaylandTestBase::SetUp();
}

void WaylandTestSimple::TearDown() {
  WaylandTestBase::TearDown();
}

WaylandTestSimpleWithAuraShell::WaylandTestSimpleWithAuraShell()
    : WaylandTestBase(
          {.enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}) {}

WaylandTestSimpleWithAuraShell::~WaylandTestSimpleWithAuraShell() = default;

void WaylandTestSimpleWithAuraShell::SetUp() {
  WaylandTestBase::SetUp();
}

void WaylandTestSimpleWithAuraShell ::TearDown() {
  WaylandTestBase::TearDown();
}

}  // namespace ui
