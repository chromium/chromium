// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_watcher_glib.h"

#include <glib.h>

#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

struct GLibX11Source : public GSource {
  // Note: The GLibX11Source is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  x11::Connection* connection;
  GPollFD* poll_fd;
};

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  gxsource->connection->Flush();
  gxsource->connection->ReadResponses();
  if (gxsource->connection->HasPendingResponses())
    return TRUE;
  *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  gxsource->connection->Flush();
  gxsource->connection->ReadResponses();
  return gxsource->connection->HasPendingResponses();
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  X11EventSource* x11_source = static_cast<X11EventSource*>(data);
  x11_source->DispatchXEvents();
  return TRUE;
}

GSourceFuncs XSourceFuncs = {XSourcePrepare, XSourceCheck, XSourceDispatch,
                             nullptr};

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
  g_source_unref(x_source_);
  started_ = false;
}

}  // namespace ui
