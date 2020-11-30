// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

#include <sys/socket.h>
#include <wayland-server.h>

#include <cstdlib>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"

namespace wl {

void DisplayDeleter::operator()(wl_display* display) {
  wl_display_destroy(display);
}

TestWaylandServerThread::TestWaylandServerThread()
    : Thread("test_wayland_server"),
      pause_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      resume_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
      controller_(FROM_HERE) {}

TestWaylandServerThread::~TestWaylandServerThread() {
  if (client_)
    wl_client_destroy(client_);

  // Stop watching the descriptor here to guarantee that no new events will come
  // during or after the destruction of the display.
  controller_.StopWatchingFileDescriptor();

  Resume();
  Stop();
}

bool TestWaylandServerThread::Start(uint32_t shell_version) {
  display_.reset(wl_display_create());
  if (!display_)
    return false;
  event_loop_ = wl_display_get_event_loop(display_.get());

  int fd[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd) < 0)
    return false;
  base::ScopedFD server_fd(fd[0]);
  base::ScopedFD client_fd(fd[1]);

  if (wl_display_init_shm(display_.get()) < 0)
    return false;
  if (!compositor_.Initialize(display_.get()))
    return false;
  if (!sub_compositor_.Initialize(display_.get()))
    return false;
  if (!viewporter_.Initialize(display_.get()))
    return false;
  if (!output_.Initialize(display_.get()))
    return false;
  SetupOutputs();

  if (!data_device_manager_.Initialize(display_.get()))
    return false;
  if (!seat_.Initialize(display_.get()))
    return false;
  if (shell_version == 6) {
    if (!zxdg_shell_v6_.Initialize(display_.get()))
      return false;
  } else if (shell_version == 7) {
    if (!xdg_shell_.Initialize(display_.get()))
      return false;
  } else {
    NOTREACHED() << "Unsupported shell version: " << shell_version;
  }
  if (!zwp_text_input_manager_v1_.Initialize(display_.get()))
    return false;
  if (!zwp_linux_dmabuf_v1_.Initialize(display_.get()))
    return false;

  client_ = wl_client_create(display_.get(), server_fd.release());
  if (!client_)
    return false;

  base::Thread::Options options;
  options.message_pump_factory = base::BindRepeating(
      &TestWaylandServerThread::CreateMessagePump, base::Unretained(this));
  if (!base::Thread::StartWithOptions(options))
    return false;

  setenv("WAYLAND_SOCKET", base::NumberToString(client_fd.release()).c_str(),
         1);

  return true;
}

void TestWaylandServerThread::Pause() {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestWaylandServerThread::DoPause,
                                         base::Unretained(this)));
  pause_event_.Wait();
}

void TestWaylandServerThread::Resume() {
  if (display_)
    wl_display_flush_clients(display_.get());
  resume_event_.Signal();
}

MockWpPresentation* TestWaylandServerThread::EnsureWpPresentation() {
  if (wp_presentation_.Initialize(display_.get()))
    return &wp_presentation_;
  return nullptr;
}

// By default, just make sure primary screen has bounds set. Otherwise delegates
// it, making it possible to emulate different scenarios, such as, multi-screen,
// lazy configuration, arbitrary ordering of the outputs metadata sending, etc.
void TestWaylandServerThread::SetupOutputs() {
  if (output_delegate_) {
    output_delegate_->SetupOutputs(&output_);
    return;
  }
  if (output_.GetRect().IsEmpty())
    output_.SetRect(gfx::Rect{0, 0, 800, 600});
}

void TestWaylandServerThread::DoPause() {
  base::RunLoop().RunUntilIdle();
  pause_event_.Signal();
  resume_event_.Wait();
}

std::unique_ptr<base::MessagePump>
TestWaylandServerThread::CreateMessagePump() {
  auto pump = std::make_unique<base::MessagePumpLibevent>();
  pump->WatchFileDescriptor(wl_event_loop_get_fd(event_loop_), true,
                            base::MessagePumpLibevent::WATCH_READ, &controller_,
                            this);
  return std::move(pump);
}

void TestWaylandServerThread::OnFileCanReadWithoutBlocking(int fd) {
  wl_event_loop_dispatch(event_loop_, 0);
  if (display_)
    wl_display_flush_clients(display_.get());
}

void TestWaylandServerThread::OnFileCanWriteWithoutBlocking(int fd) {}

}  // namespace wl
