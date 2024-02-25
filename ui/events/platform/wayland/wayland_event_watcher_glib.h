// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_GLIB_H_
#define UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_GLIB_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/platform/wayland/wayland_event_watcher.h"

using GPollFD = struct _GPollFD;
using GSource = struct _GSource;

namespace ui {

// WaylandEventWatcher implementation that uses Glib for event polling.
// The event loop integration is done according to the manual available at
// https://bit.ly/3tCjobF
class WaylandEventWatcherGlib : public WaylandEventWatcher {
 public:
  WaylandEventWatcherGlib(wl_display* display,
                          wl_event_queue* event_queue,
                          bool use_threaded_polling);
  WaylandEventWatcherGlib(const WaylandEventWatcherGlib&) = delete;
  WaylandEventWatcherGlib& operator=(const WaylandEventWatcherGlib&) = delete;
  ~WaylandEventWatcherGlib() override;

  // Handles WatchSourcePrepare.
  bool HandlePrepare();
  // Handles WatchSourceCheck.
  void HandleCheck(bool is_io_in);
  // Handles WatchSourceDispatch.
  void HandleDispatch();

 private:
  // WaylandEventWatcher override;
  bool StartWatchingFD(int fd) override;
  void StopWatchingFD() override;

  bool started_ = false;

  // The GLib event source for Wayland events.
  raw_ptr<GSource> wayland_source_ = nullptr;

  // The poll attached to |wayland_source_|.
  std::unique_ptr<GPollFD> wayland_poll_;
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_WAYLAND_WAYLAND_EVENT_WATCHER_GLIB_H_
