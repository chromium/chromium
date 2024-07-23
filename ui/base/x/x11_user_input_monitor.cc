// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_user_input_monitor.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/future.h"

namespace ui {

XUserInputMonitor::XUserInputMonitor(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner) {}

XUserInputMonitor::~XUserInputMonitor() {
  DCHECK(!connection_);
}

void XUserInputMonitor::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  StopMonitor();
}

void XUserInputMonitor::OnEvent(const x11::Event& event) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(write_key_press_callback_);

  auto* raw = event.As<x11::Input::RawDeviceEvent>();
  if (!raw || (raw->opcode != x11::Input::RawDeviceEvent::RawKeyPress &&
               raw->opcode != x11::Input::RawDeviceEvent::RawKeyRelease)) {
    return;
  }

  EventType type = raw->opcode == x11::Input::RawDeviceEvent::RawKeyPress
                       ? EventType::kKeyPressed
                       : EventType::kKeyReleased;

  auto key_sym =
      connection_->KeycodeToKeysym(static_cast<x11::KeyCode>(raw->detail), 0);
  KeyboardCode key_code = KeyboardCodeFromXKeysym(key_sym);
  counter_.OnKeyboardEvent(type, key_code);

  // Update count value in shared memory.
  if (key_press_count_mapping_) {
    write_key_press_callback_.Run(*key_press_count_mapping_,
                                  GetKeyPressCount());
  }
}

uint32_t XUserInputMonitor::GetKeyPressCount() const {
  return counter_.GetKeyPressCount();
}

void XUserInputMonitor::StartMonitor(WriteKeyPressCallback& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  write_key_press_callback_ = callback;

  if (!connection_) {
    // TODO(jamiewalch): We should pass the connection in.
    if (auto* connection = x11::Connection::Get()) {
      connection_ = connection->Clone();
    } else {
      LOG(ERROR) << "Couldn't open X connection";
      StopMonitor();
      return;
    }
  }

  connection_->AddEventObserver(this);
  if (!connection_->xinput().present()) {
    LOG(ERROR) << "X Input extension not available.";
    StopMonitor();
    return;
  }

  x11::Input::XIEventMask mask{};
  SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyPress);
  SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyRelease);
  connection_->xinput().XISelectEvents(
      {connection_->default_root(),
       {{x11::Input::DeviceId::AllMaster, {mask}}}});
  connection_->Flush();

  // Register OnConnectionData() to be called every time there is something to
  // read from |connection_|.
  watch_controller_ = base::FileDescriptorWatcher::WatchReadable(
      connection_->GetFd(),
      base::BindRepeating(&XUserInputMonitor::OnConnectionData,
                          base::Unretained(this)));

  // Start observing message loop destruction if we start monitoring the first
  // event.
  base::CurrentThread::Get()->AddDestructionObserver(this);

  // Fetch pending events if any.
  OnConnectionData();
}

void XUserInputMonitor::StartMonitorWithMapping(
    WriteKeyPressCallback& callback,
    base::WritableSharedMemoryMapping mapping) {
  StartMonitor(callback);
  key_press_count_mapping_ =
      std::make_unique<base::WritableSharedMemoryMapping>(std::move(mapping));
}

void XUserInputMonitor::StopMonitor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  watch_controller_.reset();
  connection_.reset();
  key_press_count_mapping_.reset();

  // Stop observing message loop destruction if no event is being monitored.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);
}

void XUserInputMonitor::OnConnectionData() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  connection_->DispatchAll();
}

}  // namespace ui
