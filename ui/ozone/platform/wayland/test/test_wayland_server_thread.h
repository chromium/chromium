// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_SERVER_THREAD_H_

#include <wayland-server-core.h>

#include <memory>
#include <vector>

#include "base/message_loop/message_pump_libevent.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/mock_wp_presentation.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_compositor.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_subcompositor.h"
#include "ui/ozone/platform/wayland/test/test_viewporter.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_resource;

namespace wl {

struct DisplayDeleter {
  void operator()(wl_display* display);
};

class TestWaylandServerThread : public base::Thread,
                                base::MessagePumpLibevent::FdWatcher {
 public:
  class OutputDelegate;

  TestWaylandServerThread();
  ~TestWaylandServerThread() override;

  // Starts the test Wayland server thread. If this succeeds, the WAYLAND_SOCKET
  // environment variable will be set to the string representation of a file
  // descriptor that a client can connect to. The caller is responsible for
  // ensuring that this file descriptor gets closed (for example, by calling
  // wl_display_connect).
  // Instantiates an xdg_shell of version |shell_version|; versions 6 and 7
  // (stable) are supported.
  bool Start(uint32_t shell_version);

  // Pauses the server thread when it becomes idle.
  void Pause();

  // Resumes the server thread after flushing client connections.
  void Resume();

  // Initializes and returns WpPresentation.
  MockWpPresentation* EnsureWpPresentation();

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
  TestZwpTextInputManagerV1* text_input_manager_v1() {
    return &zwp_text_input_manager_v1_;
  }
  MockZwpLinuxDmabufV1* zwp_linux_dmabuf_v1() { return &zwp_linux_dmabuf_v1_; }

  wl_display* display() const { return display_.get(); }

  void set_output_delegate(OutputDelegate* delegate) {
    output_delegate_ = delegate;
  }

 private:
  void SetupOutputs();
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
  TestCompositor compositor_;
  TestSubCompositor sub_compositor_;
  TestViewporter viewporter_;
  TestDataDeviceManager data_device_manager_;
  TestOutput output_;
  TestSeat seat_;
  MockXdgShell xdg_shell_;
  MockZxdgShellV6 zxdg_shell_v6_;
  TestZwpTextInputManagerV1 zwp_text_input_manager_v1_;
  MockZwpLinuxDmabufV1 zwp_linux_dmabuf_v1_;
  MockWpPresentation wp_presentation_;

  std::vector<std::unique_ptr<GlobalObject>> globals_;

  base::MessagePumpLibevent::FdWatchController controller_;

  OutputDelegate* output_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWaylandServerThread);
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
