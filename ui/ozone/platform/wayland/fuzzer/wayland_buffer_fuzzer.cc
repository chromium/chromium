// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This fuzzer tests browser-side implementation of
// ozone::mojom::WaylandConnection.

#include <drm_fourcc.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/wayland_connection_test_api.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using testing::_;

namespace {

using TerminateGpuCallback = base::OnceCallback<void(std::string)>;

// Copied from ui/ozone/test/mock_platform_window_delegate.h to avoid
// dependency from the whole library (it causes link problems).
class MockPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate() = default;

  MockPlatformWindowDelegate(const MockPlatformWindowDelegate&) = delete;
  MockPlatformWindowDelegate& operator=(const MockPlatformWindowDelegate&) =
      delete;

  ~MockPlatformWindowDelegate() override = default;

  MOCK_METHOD1(OnBoundsChanged, void(const BoundsChange& change));
  MOCK_METHOD1(OnDamageRect, void(const gfx::Rect& damaged_region));
  MOCK_METHOD1(DispatchEvent, void(ui::Event* event));
  MOCK_METHOD0(OnCloseRequest, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD2(OnWindowStateChanged,
               void(ui::PlatformWindowState old_state,
                    ui::PlatformWindowState new_state));
  MOCK_METHOD0(OnLostCapture, void());
  MOCK_METHOD1(OnAcceleratedWidgetAvailable,
               void(gfx::AcceleratedWidget widget));
  MOCK_METHOD0(OnWillDestroyAcceleratedWidget, void());
  MOCK_METHOD0(OnAcceleratedWidgetDestroyed, void());
  MOCK_METHOD1(OnActivationChanged, void(bool active));
  MOCK_METHOD0(OnMouseEnter, void());
};

struct Environment {
  Environment()
      : task_environment((base::CommandLine::Init(0, nullptr),
                          TestTimeouts::Initialize(),
                          base::test::TaskEnvironment::MainThreadType::UI)) {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    mojo::core::Init();
  }

  void SetTerminateGpuCallback(ui::WaylandBufferManagerHost* host) {
    DCHECK(host);
    host->SetTerminateGpuCallback(base::BindOnce(
        &Environment::OnTerminateCallbackFired, base::Unretained(this)));
  }

  void OnTerminateCallbackFired(std::string message) { terminated = true; }

  base::test::TaskEnvironment task_environment;
  bool terminated = false;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  DCHECK(!env.terminated);

  // Required for ICU initialization.
  static base::NoDestructor<base::AtExitManager> exit_manager;
  FuzzedDataProvider data_provider(data, size);

  base::CommandLine::Init(0, nullptr);

  // Required for base::FormatNumber that WaylandBufferManagerHost uses.
  base::test::InitializeICUForTesting();

  std::vector<uint32_t> known_fourccs{
      DRM_FORMAT_R8,          DRM_FORMAT_GR88,        DRM_FORMAT_ABGR8888,
      DRM_FORMAT_XBGR8888,    DRM_FORMAT_ARGB8888,    DRM_FORMAT_XRGB8888,
      DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_RGB565,
      DRM_FORMAT_NV12,        DRM_FORMAT_YVU420};

  wl::TestWaylandServerThread server;
  CHECK(server.Start());

  std::unique_ptr<ui::WaylandConnection> connection =
      std::make_unique<ui::WaylandConnection>();
  CHECK(connection->Initialize());

  // Wait until everything is initialised.
  env.task_environment.RunUntilIdle();

  auto screen = connection->wayland_output_manager()->CreateWaylandScreen();
  connection->wayland_output_manager()->InitWaylandScreen(screen.get());

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(testing::SaveArg<0>(&widget));
  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = ui::PlatformWindowType::kWindow;
  std::unique_ptr<ui::WaylandWindow> window = ui::WaylandWindow::Create(
      &delegate, connection.get(), std::move(properties));

  CHECK_NE(widget, gfx::kNullAcceleratedWidget);

  // Let the server process the events and wait until everything is initialised.
  ui::WaylandConnectionTestApi test_api(connection.get());
  test_api.SyncDisplay();

  base::FilePath temp_dir, temp_path;
  base::ScopedFD fd =
      base::CreateAndOpenFdForTemporaryFileInDir(temp_dir, &temp_path);
  EXPECT_TRUE(fd.is_valid());

  // 10K screens are reality these days.
  const gfx::Size buffer_size(data_provider.ConsumeIntegralInRange(1U, 20000U),
                              data_provider.ConsumeIntegralInRange(1U, 20000U));
  // The buffer manager opens a file descriptor for each plane so |plane_count|
  // cannot be really large.  Technically, the maximum is |ulimit| minus number
  // of file descriptors opened by this process already (which is 17 at the time
  // of writing) but there is little sense in having more than a few planes in a
  // real system so here is a hard limit of 500.
  const uint32_t kPlaneCount = data_provider.ConsumeIntegralInRange(1U, 500U);
  const uint32_t kFormat = known_fourccs[data_provider.ConsumeIntegralInRange(
      0UL, static_cast<unsigned long>(known_fourccs.size() - 1))];

  std::vector<uint32_t> strides(kPlaneCount);
  std::vector<uint32_t> offsets(kPlaneCount);
  std::vector<uint64_t> modifiers(kPlaneCount);
  for (uint32_t i = 0; i < kPlaneCount; ++i) {
    strides[i] = data_provider.ConsumeIntegralInRange(1U, UINT_MAX);
    offsets[i] = data_provider.ConsumeIntegralInRange(0U, UINT_MAX);
    modifiers[i] =
        data_provider.ConsumeIntegralInRange(uint64_t(0), UINT64_MAX);
    if (kPlaneCount > 1 && modifiers[i] == DRM_FORMAT_MOD_INVALID)
      modifiers[i] = 0;
  }

  const uint32_t kBufferId = 1;

  auto* manager_host = connection->buffer_manager_host();
  env.SetTerminateGpuCallback(manager_host);
  manager_host->CreateDmabufBasedBuffer(
      mojo::PlatformHandle(std::move(fd)), buffer_size, strides, offsets,
      modifiers, kFormat, kPlaneCount, kBufferId);

  // Wait until the buffers are created.
  test_api.SyncDisplay();

  if (!env.terminated) {
    server.RunAndWait(
        base::BindLambdaForTesting([](wl::TestWaylandServerThread* server) {
          // The server must notify the buffers are created so that the client
          // is able to free the resources (destroy the params).
          auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
          // To ensure, no other buffers are created, test the size of the
          // vector.
          for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
            zwp_linux_buffer_params_v1_send_created(
                mock_params->resource(), mock_params->buffer_resource());
          }
        }));

    test_api.SyncDisplay();
  } else {
    // If the |manager_host| fires the terminate gpu callback, we need to set
    // the callback again.
    env.SetTerminateGpuCallback(manager_host);
  }

  manager_host->DestroyBuffer(kBufferId);

  // Wait until the buffers are destroyed.
  test_api.SyncDisplay();

  // Reset the value as |env| is a static object.
  env.terminated = false;

  return 0;
}
