// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_watcher_fdwatch.h"

#include "base/task/current_thread.h"

namespace ui {

X11EventWatcherFdWatch::X11EventWatcherFdWatch(X11EventSource* source)
    : event_source_(source), watcher_controller_(FROM_HERE) {}

X11EventWatcherFdWatch::~X11EventWatcherFdWatch() {
  StopWatching();
}

void X11EventWatcherFdWatch::StartWatching() {
  if (started_ || !base::CurrentThread::Get())
    return;

  DCHECK(event_source_->connection()) << "Unable to get connection to X server";

  int fd = event_source_->connection()->GetFd();
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd, true, base::MessagePumpForUI::WATCH_READ, &watcher_controller_, this);
  started_ = true;
}

void X11EventWatcherFdWatch::StopWatching() {
  if (!started_)
    return;

  watcher_controller_.StopWatchingFileDescriptor();
  started_ = false;
}

void X11EventWatcherFdWatch::OnFileCanReadWithoutBlocking(int fd) {
  event_source_->DispatchXEvents();
}

void X11EventWatcherFdWatch::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace ui
