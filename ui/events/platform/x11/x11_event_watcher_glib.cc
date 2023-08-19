// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_watcher_glib.h"

#include <glib.h>

#include "base/memory/raw_ptr.h"

namespace ui {

namespace {

struct GLibX11Source : public GSource {
  // Note: The GLibX11Source is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  raw_ptr<x11::Connection> connection;
  raw_ptr<GPollFD> poll_fd;
};

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  // Set an infinite timeout.
  *timeout_ms = -1;

  // This function is called before polling the FD, so a flush is mandatory
  // in case:
  //   1. This is the first message loop iteration and we have unflushed
  //      requests.
  //   2. A request was made after XSourceDispatch() when running tasks from
  //      the task queue.
  auto* connection = static_cast<GLibX11Source*>(source)->connection.get();
  connection->Flush();

  // Read a pre-buffered response if available to prevent a deadlock where we
  // poll() for data that will never arrive since we already have data in our
  // read buffer.
  connection->ReadResponse(true);

  // Return true if we can determine that event processing is necessary without
  // polling the FD.
  return connection->HasPendingResponses();
}

gboolean XSourceCheck(GSource* source) {
  // Only read a response if poll() determined the FD is readable.
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  if (gxsource->poll_fd->revents & G_IO_IN)
    gxsource->connection->ReadResponse(false);
  return gxsource->connection->HasPendingResponses();
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  auto* connection = static_cast<GLibX11Source*>(source)->connection.get();
  connection->Dispatch();

  // Flushing here is not strictly required, but when this function returns,
  // tasks from the task queue will be run, which may take some time.  Flushing
  // now will ensure screen updates occur right away.  Fortunately, this won't
  // do any syscalls if it's not necessary.
  connection->Flush();

  // Don't remove the GLibX11Source from the main loop.
  return G_SOURCE_CONTINUE;
}

void XSourceFinalize(GSource* source) {
  GLibX11Source* src = static_cast<GLibX11Source*>(source);
  src->connection = nullptr;
  src->poll_fd = nullptr;
}

GSourceFuncs XSourceFuncs = {XSourcePrepare, XSourceCheck, XSourceDispatch,
                             XSourceFinalize};

}  // namespace

X11EventWatcherGlib::X11EventWatcherGlib(X11EventSource* source)
    : event_source_(source) {}

X11EventWatcherGlib::~X11EventWatcherGlib() {
  StopWatching();
}

void X11EventWatcherGlib::StartWatching() {
  if (started_)
    return;

  auto* connection = event_source_->connection();
  if (!connection->Ready())
    return;

  x_poll_ = std::make_unique<GPollFD>();
  x_poll_->fd = connection->GetFd();
  x_poll_->events = G_IO_IN;
  x_poll_->revents = 0;

  GLibX11Source* glib_x_source = static_cast<GLibX11Source*>(
      g_source_new(&XSourceFuncs, sizeof(GLibX11Source)));
  glib_x_source->connection = x11::Connection::Get();
  glib_x_source->poll_fd = x_poll_.get();

  x_source_ = glib_x_source;
  g_source_add_poll(x_source_, x_poll_.get());
  g_source_set_can_recurse(x_source_, TRUE);
  g_source_set_callback(x_source_, nullptr, event_source_, nullptr);
  auto* context = g_main_context_get_thread_default();
  if (!context)
    context = g_main_context_default();
  g_source_attach(x_source_, context);
  started_ = true;
}

void X11EventWatcherGlib::StopWatching() {
  if (!started_)
    return;

  g_source_destroy(x_source_);
  // `g_source_unref` decreases the reference count on `x_source_`. The
  // underlying memory is freed if the reference count goes to zero. We use
  // ExtractAsDangling() here to avoid holding a briefly dangling ptr in case
  // the memory is freed.
  g_source_unref(x_source_.ExtractAsDangling());
  started_ = false;
}

}  // namespace ui
