// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_USER_INPUT_MONITOR_H_
#define UI_OZONE_PUBLIC_PLATFORM_USER_INPUT_MONITOR_H_


#include "base/component_export.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/task/single_thread_task_runner.h"

namespace ui {

// User input monitor for multimedia streams.
//
// Monitors the keyboard input events in the browser process.  Can be provided
// with the shared memory mapping; in such a case should call the callback
// provided via StartMonitor() or StartMonitorWithMapping() every time
// the user presses a key.
class COMPONENT_EXPORT(OZONE_BASE) PlatformUserInputMonitor {
 public:
  // Via this callback, the platform implementation of the monitor gets the
  // WriteKeyPressMonitorCount defined in media/base/user_input_monitor.h.
  using WriteKeyPressCallback = base::RepeatingCallback<
      void(const base::WritableSharedMemoryMapping& shmem, uint32_t count)>;

  PlatformUserInputMonitor();
  PlatformUserInputMonitor(const PlatformUserInputMonitor&) = delete;
  PlatformUserInputMonitor& operator=(const PlatformUserInputMonitor&) = delete;
  virtual ~PlatformUserInputMonitor();

  // Returns how many key presses happened since this monitor has been started.
  virtual uint32_t GetKeyPressCount() const = 0;
  // Starts monitoring of events without a mapping.
  virtual void StartMonitor(WriteKeyPressCallback callback) = 0;
  // Starts monitoring of events.  The |mapping| should be passed to the
  // |callback| upon invocation.
  virtual void StartMonitorWithMapping(
      WriteKeyPressCallback callback,
      base::WritableSharedMemoryMapping mapping) = 0;
  // Stops monitoring.
  virtual void StopMonitor() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_USER_INPUT_MONITOR_H_
