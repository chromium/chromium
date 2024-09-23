// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/platform/x11/x11_event_watcher_fdwatch.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/task/current_thread.h"

namespace ui {

X11EventWatcherFdWatch::X11EventWatcherFdWatch(X11EventSource* source)
    : event_source_(source),
      connection_watcher_(FROM_HERE),
      pipe_watcher_(FROM_HERE) {}

X11EventWatcherFdWatch::~X11EventWatcherFdWatch() {
  StopWatching();
}

void X11EventWatcherFdWatch::StartWatching() {
  if (started_ || !base::CurrentThread::Get())
    return;

  DCHECK(event_source_->connection()) << "Unable to get connection to X server";

  int fd = event_source_->connection()->GetFd();
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd, true, base::MessagePumpForUI::WATCH_READ, &connection_watcher_, this);

  PCHECK(pipe2(pipe_, O_CLOEXEC | O_NONBLOCK) != -1);
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      pipe_[0], true, base::MessagePumpForUI::WATCH_READ, &pipe_watcher_, this);
  started_ = true;
}

void X11EventWatcherFdWatch::StopWatching() {
  if (!started_)
    return;

  connection_watcher_.StopWatchingFileDescriptor();
  pipe_watcher_.StopWatchingFileDescriptor();
  for (int& pfd : pipe_) {
    PCHECK(close(pfd) != -1);
    pfd = -1;
  }
  started_ = false;
}

void X11EventWatcherFdWatch::OnFileCanReadWithoutBlocking(int fd) {
  // Drain the pipe up to 128 bytes at a time (this should be enough to
  // empty it under normal conditions in a single syscall).
  char buf[128];
  while (read(pipe_[0], &buf, sizeof(buf)) != -1) {
  }
  PCHECK(errno == EAGAIN);

  // Dispatch a single event.
  auto* connection = event_source_->connection();
  if (!connection->HasPendingResponses())
    connection->ReadResponses();
  connection->Dispatch();
  connection->Flush();

  // We may deadlock if there are more events to dispatch but the socket is not
  // readable. To prevent this, write a byte to the pipe so that the message
  // loop will wake up and dispatch the event. This will cause the event to be
  // dispatched with the same priority instead of being delayed if PostTask was
  // used.
  if (connection->HasPendingResponses())
    PCHECK(write(pipe_[1], "a", 1) != -1 || errno == EAGAIN);
}

void X11EventWatcherFdWatch::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace ui
