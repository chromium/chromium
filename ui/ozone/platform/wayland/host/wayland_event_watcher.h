// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_

#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"

struct wl_display;

namespace ui {

// WaylandEventWatcher serves a single purpose: poll for events in the wayland
// connection file descriptor. Which will then trigger input objects (e.g:
// WaylandPointer, WaylandKeyboard, etc) callbacks, indirectly leading to calls
// into WaylandEventSource, so feeding the platform events pipeline.
class WaylandEventWatcher : public base::MessagePumpForUI::FdWatcher {
 public:
  explicit WaylandEventWatcher(wl_display* display);
  WaylandEventWatcher(const WaylandEventWatcher&) = delete;
  WaylandEventWatcher& operator=(const WaylandEventWatcher&) = delete;
  ~WaylandEventWatcher() override;

  // Starts polling for events from the wayland connection file descriptor.
  // This method assumes connection is already estabilished and input objects
  // are already bound and properly initialized.
  bool StartProcessingEvents();

  // Stops polling for events from input devices.
  bool StopProcessingEvents();

 private:
  // base::MessagePumpForUI::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  bool StartWatchingFd(base::WatchableIOMessagePumpPosix::Mode mode);
  void MaybePrepareReadQueue();

  base::MessagePumpForUI::FdWatchController controller_;

  wl_display* const display_;  // Owned by WaylandConnection.

  bool watching_ = false;
  bool prepared_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
