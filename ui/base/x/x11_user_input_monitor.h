// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_USER_INPUT_MONITOR_H_
#define UI_BASE_X_X11_USER_INPUT_MONITOR_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/keyboard_event_counter.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xinput.h"

namespace ui {

// This is the actual implementation of event monitoring. It's separated from
// UserInputMonitorLinux since it needs to be deleted on the IO thread.
class COMPONENT_EXPORT(UI_BASE_X) XUserInputMonitor
    : public base::CurrentThread::DestructionObserver,
      public x11::EventObserver {
 public:
  using WriteKeyPressCallback = base::RepeatingCallback<
      void(const base::WritableSharedMemoryMapping& shmem, uint32_t count)>;

  explicit XUserInputMonitor(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  XUserInputMonitor(const XUserInputMonitor&) = delete;
  XUserInputMonitor& operator=(const XUserInputMonitor&) = delete;
  ~XUserInputMonitor() override;

  uint32_t GetKeyPressCount() const;
  void StartMonitor(WriteKeyPressCallback& callback);
  void StartMonitorWithMapping(WriteKeyPressCallback& callback,
                               base::WritableSharedMemoryMapping mapping);
  void StopMonitor();

 private:
  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  void OnConnectionData();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Used for sharing key press count value.
  std::unique_ptr<base::WritableSharedMemoryMapping> key_press_count_mapping_;

  // The following members should only be accessed on the IO thread.
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_controller_;
  std::unique_ptr<x11::Connection> connection_;
  KeyboardEventCounter counter_;

  WriteKeyPressCallback write_key_press_callback_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_USER_INPUT_MONITOR_H_
