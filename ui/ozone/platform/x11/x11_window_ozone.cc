// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate)
    : X11Window(delegate) {}

X11WindowOzone::~X11WindowOzone() {
  PrepareForShutdown();
  Close();
}

void X11WindowOzone::PrepareForShutdown() {
  DCHECK(X11EventSource::GetInstance());
  X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XWindow::SetCursor(cursor_ozone->xcursor());
}

// CheckCanDispatchNextPlatformEvent is called by X11EventSourceLibevent to
// determine whether X11WindowOzone instance (XEventDispatcher implementation)
// is able to process next translated event sent by it. So, it's done through
// |handle_next_event_| internal flag, used in subsequent CanDispatchEvent
// call.
void X11WindowOzone::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  if (is_shutting_down())
    return;

  handle_next_event_ = XWindow::IsTargetedBy(*xev);
}

void X11WindowOzone::PlatformEventDispatchFinished() {
  handle_next_event_ = false;
}

PlatformEventDispatcher* X11WindowOzone::GetPlatformEventDispatcher() {
  return this;
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!XWindow::IsTargetedBy(*xev))
    return false;

  XWindow::ProcessEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(XWindow::window(), x11::None);
  return handle_next_event_;
}

void X11WindowOzone::SetPlatformEventDispatcher() {
  DCHECK(X11EventSource::GetInstance());
  X11EventSource::GetInstance()->AddXEventDispatcher(this);
}

}  // namespace ui
