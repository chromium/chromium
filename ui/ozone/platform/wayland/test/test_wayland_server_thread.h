// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_

#include <wayland-server-core.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "base/message_loop/message_pump_libevent.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/mock_wp_presentation.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_alpha_compositing.h"
#include "ui/ozone/platform/wayland/test/test_compositor.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_subcompositor.h"
#include "ui/ozone/platform/wayland/test/test_surface_augmenter.h"
#include "ui/ozone/platform/wayland/test/test_viewporter.h"
#include "ui/ozone/platform/wayland/test/test_wp_pointer_gestures.h"
#include "ui/ozone/platform/wayland/test/test_zcr_stylus.h"
#include "ui/ozone/platform/wayland/test/test_zcr_text_input_extension.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_explicit_synchronization.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_resource;

namespace wl {

struct DisplayDeleter {
  void operator()(wl_display* display);
};

// Server configuration related enums and structs.
enum class ShellVersion { kV6, kStable };
enum class PrimarySelectionProtocol { kNone, kGtk, kZwp };
enum class CompositorVersion { kV3, kV4 };
enum class ShouldUseExplicitSynchronizationProtocol { kNone, kUse };

struct ServerConfig {
  ShellVersion shell_version = ShellVersion::kStable;
  CompositorVersion compositor_version = CompositorVersion::kV4;
  PrimarySelectionProtocol primary_selection_protocol =
      PrimarySelectionProtocol::kNone;
  ShouldUseExplicitSynchronizationProtocol use_explicit_synchronization =
      ShouldUseExplicitSynchronizationProtocol::kUse;
};

class TestSelectionDeviceManager;

class TestWaylandServerThread : public base::Thread,
                                base::MessagePumpLibevent::FdWatcher {
 public:
  class OutputDelegate;

  TestWaylandServerThread();

  TestWaylandServerThread(const TestWaylandServerThread&) = delete;
  TestWaylandServerThread& operator=(const TestWaylandServerThread&) = delete;

  ~TestWaylandServerThread() override;

  // Starts the test Wayland server thread. If this succeeds, the WAYLAND_SOCKET
  // environment variable will be set to the string representation of a file
  // descriptor that a client can connect to. The caller is responsible for
  // ensuring that this file descriptor gets closed (for example, by calling
  // wl_display_connect).
  // Instantiates an xdg_shell of version |shell_version|; versions 6 and 7
  // (stable) are supported.
  bool Start(const ServerConfig& config);

  // Pauses the server thread when it becomes idle.
  void Pause();

  // Resumes the server thread after flushing client connections.
  void Resume();

  // Initializes and returns WpPresentation.
  MockWpPresentation* EnsureWpPresentation();
  // Initializes and returns SurfaceAugmenter.
  TestSurfaceAugmenter* EnsureSurfaceAugmenter();

  template <typename T>
  T* GetObject(uint32_t id) {
    wl_resource* resource = wl_client_get_object(client_, id);
    return resource ? T::FromResource(resource) : nullptr;
  }

  TestOutput* CreateAndInitializeOutput() {
    auto output = std::make_unique<TestOutput>();
    output->Initialize(display());

    TestOutput* output_ptr = output.get();
    globals_.push_back(std::move(output));
    return output_ptr;
  }

  TestDataDeviceManager* data_device_manager() { return &data_device_manager_; }
  TestSeat* seat() { return &seat_; }
  MockXdgShell* xdg_shell() { return &xdg_shell_; }
  TestOutput* output() { return &output_; }
  TestZcrTextInputExtensionV1* text_input_extension_v1() {
    return &zcr_text_input_extension_v1_;
  }
  TestZwpTextInputManagerV1* text_input_manager_v1() {
    return &zwp_text_input_manager_v1_;
  }
  TestZwpLinuxExplicitSynchronizationV1*
  zwp_linux_explicit_synchronization_v1() {
    return &zwp_linux_explicit_synchronization_v1_;
  }
  MockZwpLinuxDmabufV1* zwp_linux_dmabuf_v1() { return &zwp_linux_dmabuf_v1_; }

  wl_display* display() const { return display_.get(); }

  TestSelectionDeviceManager* primary_selection_device_manager() {
    return primary_selection_device_manager_.get();
  }

  TestWpPointerGestures& wp_pointer_gestures() { return wp_pointer_gestures_; }

  void set_output_delegate(OutputDelegate* delegate) {
    output_delegate_ = delegate;
  }

  wl_client* client() const { return client_; }

 private:
  void SetupOutputs();
  bool SetupPrimarySelectionManager(PrimarySelectionProtocol protocol);
  bool SetupExplicitSynchronizationProtocol(
      ShouldUseExplicitSynchronizationProtocol usage);
  void DoPause();

  std::unique_ptr<base::MessagePump> CreateMessagePump();

  // base::MessagePumpLibevent::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  std::unique_ptr<wl_display, DisplayDeleter> display_;
  wl_client* client_ = nullptr;
  wl_event_loop* event_loop_ = nullptr;

  base::WaitableEvent pause_event_;
  base::WaitableEvent resume_event_;

  // Represent Wayland global objects
  // Compositor version is selected dynamically by server config but version is
  // actually set on construction thus both compositor version objects appear
  // here.
  // TODO(crbug.com/1315587): Refactor this pattern when required.
  TestCompositor compositor_v4_;
  TestCompositor compositor_v3_;
  TestSubCompositor sub_compositor_;
  TestViewporter viewporter_;
  TestAlphaCompositing alpha_compositing_;
  TestDataDeviceManager data_device_manager_;
  TestOutput output_;
  TestOverlayPrioritizer overlay_prioritizer_;
  TestSurfaceAugmenter surface_augmenter_;
  TestSeat seat_;
  MockXdgShell xdg_shell_;
  MockZxdgShellV6 zxdg_shell_v6_;
  TestZcrStylus zcr_stylus_;
  TestZcrTextInputExtensionV1 zcr_text_input_extension_v1_;
  TestZwpTextInputManagerV1 zwp_text_input_manager_v1_;
  TestZwpLinuxExplicitSynchronizationV1 zwp_linux_explicit_synchronization_v1_;
  MockZwpLinuxDmabufV1 zwp_linux_dmabuf_v1_;
  MockWpPresentation wp_presentation_;
  TestWpPointerGestures wp_pointer_gestures_;
  std::unique_ptr<TestSelectionDeviceManager> primary_selection_device_manager_;

  std::vector<std::unique_ptr<GlobalObject>> globals_;

  base::MessagePumpLibevent::FdWatchController controller_;

  OutputDelegate* output_delegate_ = nullptr;
};

class TestWaylandServerThread::OutputDelegate {
 public:
  // Tests may implement this such that it emulates different display/output
  // test scenarios. For example, multi-screen, lazy configuration, arbitrary
  // ordering of the outputs metadata events, etc.
  virtual void SetupOutputs(TestOutput* primary_output) = 0;

 protected:
  virtual ~OutputDelegate() = default;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_
