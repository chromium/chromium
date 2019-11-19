// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source_glib.h"

#include <glib.h>
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

struct GLibX11Source : public GSource {
  // Note: The GLibX11Source is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  XDisplay* display;
  GPollFD* poll_fd;
};

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  if (XPending(gxsource->display))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  return XPending(gxsource->display);
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  X11EventSource* x11_source = static_cast<X11EventSource*>(data);
  x11_source->DispatchXEvents();
  return TRUE;
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

}  // namespace

X11EventSourceGlib::X11EventSourceGlib(XDisplay* display)
    : event_source_(this, display) {
  InitXSource(ConnectionNumber(display));
}

X11EventSourceGlib::~X11EventSourceGlib() {
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
}

void X11EventSourceGlib::ProcessXEvent(XEvent* xevent) {
  DispatchEvent(xevent);
}

void X11EventSourceGlib::StopCurrentEventStream() {
  event_source_.StopCurrentEventStream();
}

void X11EventSourceGlib::OnDispatcherListChanged() {
  event_source_.OnDispatcherListChanged();
}

void X11EventSourceGlib::InitXSource(int fd) {
  DCHECK(!x_source_);
  DCHECK(event_source_.display()) << "Unable to get connection to X server";

  x_poll_ = std::make_unique<GPollFD>();
  x_poll_->fd = fd;
  x_poll_->events = G_IO_IN;
  x_poll_->revents = 0;

  GLibX11Source* glib_x_source = static_cast<GLibX11Source*>(
      g_source_new(&XSourceFuncs, sizeof(GLibX11Source)));
  glib_x_source->display = event_source_.display();
  glib_x_source->poll_fd = x_poll_.get();

  x_source_ = glib_x_source;
  g_source_add_poll(x_source_, x_poll_.get());
  g_source_set_can_recurse(x_source_, TRUE);
  g_source_set_callback(x_source_, NULL, &event_source_, NULL);
  g_source_attach(x_source_, g_main_context_default());
}

// static
std::unique_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return std::make_unique<X11EventSourceGlib>(gfx::GetXDisplay());
}

}  // namespace ui
