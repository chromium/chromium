// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_user_input_monitor.h"

#include "base/task/single_thread_task_runner.h"
#include "ui/base/x/x11_user_input_monitor.h"

namespace ui {

X11UserInputMonitor::X11UserInputMonitor(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : user_input_monitor_(
          std::make_unique<ui::XUserInputMonitor>(io_task_runner)) {}

X11UserInputMonitor::~X11UserInputMonitor() = default;

uint32_t X11UserInputMonitor::GetKeyPressCount() const {
  return user_input_monitor_->GetKeyPressCount();
}

void X11UserInputMonitor::StartMonitor(
    PlatformUserInputMonitor::WriteKeyPressCallback callback) {
  user_input_monitor_->StartMonitor(callback);
}

void X11UserInputMonitor::StartMonitorWithMapping(
    PlatformUserInputMonitor::WriteKeyPressCallback callback,
    base::WritableSharedMemoryMapping mapping) {
  user_input_monitor_->StartMonitorWithMapping(callback, std::move(mapping));
}

void X11UserInputMonitor::StopMonitor() {
  user_input_monitor_->StopMonitor();
}

}  // namespace ui
