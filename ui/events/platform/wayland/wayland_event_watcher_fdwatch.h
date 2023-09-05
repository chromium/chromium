// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_FDWATCH_H_
#define UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_FDWATCH_H_

#include "base/message_loop/message_pump_for_ui.h"
#include "ui/events/platform/wayland/wayland_event_watcher.h"

namespace ui {

// WaylandEventWatcherFdWatch implementation that uses Glib for event polling.
class WaylandEventWatcherFdWatch : public WaylandEventWatcher,
                                   public base::MessagePumpForUI::FdWatcher {
 public:
  WaylandEventWatcherFdWatch(wl_display* display,
                             wl_event_queue* event_queue,
                             bool use_threaded_polling);
  WaylandEventWatcherFdWatch(const WaylandEventWatcherFdWatch&) = delete;
  WaylandEventWatcherFdWatch& operator=(const WaylandEventWatcherFdWatch&) =
      delete;
  ~WaylandEventWatcherFdWatch() override;

 private:
  // WaylandEventWatcher override;
  bool StartWatchingFD(int fd) override;
  void StopWatchingFD() override;

  // base::MessagePumpForUI::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  base::MessagePumpForUI::FdWatchController controller_;
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_FDWATCH_H_
