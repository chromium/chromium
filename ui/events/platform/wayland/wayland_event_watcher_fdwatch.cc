// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/wayland/wayland_event_watcher_fdwatch.h"

#include "base/notreached.h"
#include "base/task/current_thread.h"

namespace ui {

WaylandEventWatcherFdWatch::WaylandEventWatcherFdWatch(
    wl_display* display,
    wl_event_queue* event_queue,
    bool use_threaded_polling)
    : WaylandEventWatcher(display, event_queue, use_threaded_polling),
      controller_(FROM_HERE) {}

WaylandEventWatcherFdWatch::~WaylandEventWatcherFdWatch() {
  StopProcessingEvents();
}

bool WaylandEventWatcherFdWatch::StartWatchingFD(int fd) {
  DCHECK(base::CurrentUIThread::IsSet());
  WlDisplayPrepareToRead();
  return base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
}

void WaylandEventWatcherFdWatch::StopWatchingFD() {
  DCHECK(base::CurrentUIThread::IsSet());
  CHECK(controller_.StopWatchingFileDescriptor())
      << "Unable to stop watching the Wayland display fd.";
}

void WaylandEventWatcherFdWatch::OnFileCanReadWithoutBlocking(int fd) {
  // All the error checking and conditions to read are handled by the base
  // class. See https://bit.ly/3tCjobF to get an idea how it works.
  //
  // TODO(crbug.com/40816750): once libevent is updated to the newest version,
  // use watcher callbacks that notify clients about intention to sleep/poll or
  // about bytes available to be read so that we are able to correctly call all
  // these methods. At the moment, they are constantly called whenever there is
  // something to read, which is inefficient. See WaylandEventWatcherGlib for
  // example.
  WlDisplayReadEvents();
  WlDisplayDispatchPendingQueue();

  // If prepare failed, dispatch the events once again.
  if (!WlDisplayPrepareToRead()) {
    WlDisplayDispatchPendingQueue();
  }
}

void WaylandEventWatcherFdWatch::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED_IN_MIGRATION();
}

// static
std::unique_ptr<WaylandEventWatcher>
WaylandEventWatcher::CreateWaylandEventWatcher(wl_display* display,
                                               wl_event_queue* event_queue,
                                               bool use_threaded_polling) {
  return std::make_unique<WaylandEventWatcherFdWatch>(display, event_queue,
                                                      use_threaded_polling);
}

}  // namespace ui
