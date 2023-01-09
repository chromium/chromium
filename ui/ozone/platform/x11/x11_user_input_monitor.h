// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_USER_INPUT_MONITOR_H_
#define UI_OZONE_PLATFORM_X11_X11_USER_INPUT_MONITOR_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "ui/ozone/public/platform_user_input_monitor.h"

namespace ui {

class XUserInputMonitor;

class X11UserInputMonitor : public PlatformUserInputMonitor {
 public:
  explicit X11UserInputMonitor(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  X11UserInputMonitor(const X11UserInputMonitor&) = delete;
  X11UserInputMonitor& operator=(const X11UserInputMonitor&) = delete;
  ~X11UserInputMonitor() override;

  // PlatformUserInputMonitor:
  uint32_t GetKeyPressCount() const override;
  void StartMonitor(
      PlatformUserInputMonitor::WriteKeyPressCallback callback) override;
  void StartMonitorWithMapping(
      PlatformUserInputMonitor::WriteKeyPressCallback callback,
      base::WritableSharedMemoryMapping mapping) override;
  void StopMonitor() override;

 private:
  std::unique_ptr<XUserInputMonitor> user_input_monitor_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_USER_INPUT_MONITOR_H_
