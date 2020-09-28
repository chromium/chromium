// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/test/x11_property_change_waiter.h"

#include <utility>

#include "base/run_loop.h"
#include "ui/events/event.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

X11PropertyChangeWaiter::X11PropertyChangeWaiter(x11::Window window,
                                                 const char* property)
    : x_window_(window), property_(property), wait_(true) {
  // Ensure that we are listening to PropertyNotify events for |window|. This
  // is not the case for windows which were not created by X11Window.
  x_window_events_ = std::make_unique<XScopedEventSelector>(
      x_window_, x11::EventMask::PropertyChange);

  // Override the dispatcher so that we get events before X11Window does. We
  // must do this because X11Window stops propagation.
  dispatcher_ = X11EventSource::GetInstance()->OverrideXEventDispatcher(this);
}

X11PropertyChangeWaiter::~X11PropertyChangeWaiter() = default;

void X11PropertyChangeWaiter::Wait() {
  if (!wait_)
    return;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  dispatcher_.reset();
}

bool X11PropertyChangeWaiter::ShouldKeepOnWaiting(x11::Event* event) {
  // Stop waiting once we get a property change.
  return true;
}

bool X11PropertyChangeWaiter::DispatchXEvent(x11::Event* x11_event) {
  auto* prop = x11_event->As<x11::PropertyNotifyEvent>();
  if (!wait_ || !prop || prop->window != x_window_ ||
      prop->atom != gfx::GetAtom(property_) || ShouldKeepOnWaiting(x11_event)) {
    return false;
  }

  wait_ = false;
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
  return false;
}

}  // namespace ui
