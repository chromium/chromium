// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_

#include <wayland-server-core.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_epoll.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "ui/display/types/display_constants.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/test/mock_wp_presentation.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_activation_v1.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_toplevel_icon.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_alpha_compositing.h"
#include "ui/ozone/platform/wayland/test/test_compositor.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_fractional_scale.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_subcompositor.h"
#include "ui/ozone/platform/wayland/test/test_viewporter.h"
#include "ui/ozone/platform/wayland/test/test_wp_linux_drm_syncobj.h"
#include "ui/ozone/platform/wayland/test/test_wp_pointer_gestures.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_explicit_synchronization.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"
#include "ui/ozone/platform/wayland/test/test_zxdg_output_manager.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_resource;

namespace wl {

struct DisplayDeleter {
  void operator()(wl_display* display);
};

// Server configuration related enums and structs.
enum class PrimarySelectionProtocol { kNone, kGtk, kZwp };
enum class ShouldUseExplicitSynchronizationProtocol { kNone, kUse };
enum class ShouldUseLinuxDrmSyncobjProtocol { kNone, kUse };
// Text input protocol type.
enum class ZwpTextInputType { kV1, kV3 };

struct ServerConfig {
  ZwpTextInputType text_input_type = ZwpTextInputType::kV1;
  TestCompositor::Version compositor_version = TestCompositor::Version::kV4;
  PrimarySelectionProtocol primary_selection_protocol =
      PrimarySelectionProtocol::kNone;
  ShouldUseExplicitSynchronizationProtocol use_explicit_synchronization =
      ShouldUseExplicitSynchronizationProtocol::kUse;
  ShouldUseLinuxDrmSyncobjProtocol use_linux_drm_syncobj =
      ShouldUseLinuxDrmSyncobjProtocol::kNone;
  bool supports_viewporter_surface_scaling = false;
};

class TestWaylandServerThread;

// A custom listener that holds wl_listener and the pointer to a test_server.
struct TestServerListener {
 public:
  explicit TestServerListener(TestWaylandServerThread* server)
      : test_server(server) {}
  wl_listener listener;
  const raw_ptr<TestWaylandServerThread> test_server;
};

class TestSelectionDeviceManager;

class TestWaylandServerThread : public base::Thread,
                                base::MessagePumpEpoll::FdWatcher {
 public:
  class OutputDelegate;

  TestWaylandServerThread();
  explicit TestWaylandServerThread(const ServerConfig& config);

  TestWaylandServerThread(const TestWaylandServerThread&) = delete;
  TestWaylandServerThread& operator=(const TestWaylandServerThread&) = delete;

  ~TestWaylandServerThread() override;

  // Starts the test Wayland server thread. If this succeeds, the WAYLAND_SOCKET
  // environment variable will be set to the string representation of a file
  // descriptor that a client can connect to. The caller is responsible for
  // ensuring that this file descriptor gets closed (for example, by calling
  // wl_display_connect).
  bool Start();

  // Runs 'callback' or 'closure' on the server thread; blocks until the
  // callable is run and all pending Wayland requests and events are delivered.
  void RunAndWait(base::OnceCallback<void(TestWaylandServerThread*)> callback);
  void RunAndWait(base::OnceClosure closure);

  // Posts a 'callback' or 'closure' to the server thread.
  void Post(base::OnceCallback<void(TestWaylandServerThread*)> callback);
  void Post(base::OnceClosure closure);

  // Returns WpPresentation. If it hasn't been initialized yet, initializes that
  // first and then returns.
  MockWpPresentation* EnsureAndGetWpPresentation();

  template <typename T>
  T* GetObject(uint32_t id) {
    // All the protocol calls must be made on the correct thread.
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    wl_resource* resource = wl_client_get_object(client_, id);
    return resource ? T::FromResource(resource) : nullptr;
  }

  TestOutput* CreateAndInitializeOutput(TestOutputMetrics metrics = {}) {
    auto output = std::make_unique<TestOutput>(std::move(metrics));
    output->Initialize(display());

    TestOutput* output_ptr = output.get();
    globals_.push_back(std::move(output));
    return output_ptr;
  }

  TestDataDeviceManager* data_device_manager() { return &data_device_manager_; }
  TestSeat* seat() { return &seat_; }
  MockXdgShell* xdg_shell() { return &xdg_shell_; }
  TestOutput* output() { return &output_; }
  TestZwpTextInputManagerV1* text_input_manager_v1() {
    return &zwp_text_input_manager_v1_;
  }
  TestZwpTextInputManagerV3* text_input_manager_v3() {
    return &zwp_text_input_manager_v3_;
  }
  TestZwpLinuxExplicitSynchronizationV1*
  zwp_linux_explicit_synchronization_v1() {
    return &zwp_linux_explicit_synchronization_v1_;
  }
  TestWpLinuxDrmSyncobjManagerV1* wp_linux_drm_syncobj_manager_v1() {
    return &wp_linux_drm_syncobj_manager_v1_;
  }
  MockZwpLinuxDmabufV1* zwp_linux_dmabuf_v1() { return &zwp_linux_dmabuf_v1_; }

  wl_display* display() const { return display_.get(); }

  TestSelectionDeviceManager* primary_selection_device_manager() {
    return primary_selection_device_manager_.get();
  }

  TestWpPointerGestures& wp_pointer_gestures() { return wp_pointer_gestures_; }

  MockZcrColorManagerV1* zcr_color_manager_v1() {
    return &zcr_color_manager_v1_;
  }

  MockXdgActivationV1* xdg_activation_v1() { return &xdg_activation_v1_; }

  MockXdgToplevelIconManagerV1* xdg_toplevel_icon_manager_v1() {
    return &xdg_toplevel_icon_manager_v1_;
  }

  wl_client* client() const { return client_; }

  void OnClientDestroyed(wl_client* client);

  // Returns next available serial. Must be called on the server thread.
  uint32_t GetNextSerial() const;
  // Returns next available timestamp. Suitable for events sent from the server
  // the client. Must be called on the server thread.
  uint32_t GetNextTime();

 private:
  bool SetupPrimarySelectionManager(PrimarySelectionProtocol protocol);
  bool SetupExplicitSynchronizationProtocol(
      ShouldUseExplicitSynchronizationProtocol usage);
  bool SetupLinuxDrmSyncobjProtocol(ShouldUseLinuxDrmSyncobjProtocol usage);

  std::unique_ptr<base::MessagePump> CreateMessagePump(
      base::OnceClosure closure);

  // Executes the closure and flushes the server event queue. Must be run on
  // server's thread.
  void DoRun(base::OnceClosure closure);

  // base::MessagePumpEpoll::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // wl_protocol_logger. Whenever there is a call to a protocol from the server
  // side, the logger is invoked. This is handy as we can use this to verify all
  // the protocol calls happen only when the server thread is not running. This
  // helps to avoid thread races as the client runs on a different from the
  // server tread.
  static void ProtocolLogger(void* user_data,
                             enum wl_protocol_logger_type direction,
                             const struct wl_protocol_logger_message* message);

  std::unique_ptr<wl_display, DisplayDeleter> display_;
  TestServerListener client_destroy_listener_;
  raw_ptr<wl_client> client_ = nullptr;
  raw_ptr<wl_event_loop> event_loop_ = nullptr;
  raw_ptr<wl_protocol_logger, DanglingUntriaged> protocol_logger_ = nullptr;

  ServerConfig config_;

  // Represent Wayland global objects
  TestCompositor compositor_;

  TestSubCompositor sub_compositor_;
  TestViewporter viewporter_;
  TestFractionalScaleManager fractional_scale_manager_;
  TestAlphaCompositing alpha_compositing_;
  TestDataDeviceManager data_device_manager_;
  TestOutput output_;
  TestOverlayPrioritizer overlay_prioritizer_;
  TestSeat seat_;
  TestZXdgOutputManager zxdg_output_manager_;
  MockXdgShell xdg_shell_;
  ::testing::NiceMock<MockZcrColorManagerV1> zcr_color_manager_v1_;
  TestZwpTextInputManagerV1 zwp_text_input_manager_v1_;
  TestZwpTextInputManagerV3 zwp_text_input_manager_v3_;
  TestZwpLinuxExplicitSynchronizationV1 zwp_linux_explicit_synchronization_v1_;
  TestWpLinuxDrmSyncobjManagerV1 wp_linux_drm_syncobj_manager_v1_;
  MockZwpLinuxDmabufV1 zwp_linux_dmabuf_v1_;
  MockWpPresentation wp_presentation_;
  TestWpPointerGestures wp_pointer_gestures_;
  MockXdgActivationV1 xdg_activation_v1_;
  MockXdgToplevelIconManagerV1 xdg_toplevel_icon_manager_v1_;
  std::unique_ptr<TestSelectionDeviceManager> primary_selection_device_manager_;

  std::vector<std::unique_ptr<GlobalObject>> globals_;

  base::MessagePumpEpoll::FdWatchController controller_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<TestWaylandServerThread> weak_ptr_factory_{this};
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_
