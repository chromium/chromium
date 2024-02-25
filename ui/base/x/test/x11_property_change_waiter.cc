// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/test/x11_property_change_waiter.h"

#include <utility>

#include "base/run_loop.h"
#include "ui/events/event.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

X11PropertyChangeWaiter::X11PropertyChangeWaiter(x11::Window window,
                                                 const char* property)
    : connection_(x11::Connection::Get()),
      x_window_(window),
      property_(property),
      wait_(true) {
  // Ensure that we are listening to PropertyNotify events for |window|. This
  // is not the case for windows which were not created by X11Window.
  x_window_events_ =
      connection_->ScopedSelectEvent(x_window_, x11::EventMask::PropertyChange);

  connection_->AddEventObserver(this);
}

X11PropertyChangeWaiter::~X11PropertyChangeWaiter() {
  connection_->RemoveEventObserver(this);
}

void X11PropertyChangeWaiter::Wait() {
  if (!wait_) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool X11PropertyChangeWaiter::ShouldKeepOnWaiting() {
  // Stop waiting once we get a property change.
  return true;
}

void X11PropertyChangeWaiter::OnEvent(const x11::Event& x11_event) {
  auto* prop = x11_event.As<x11::PropertyNotifyEvent>();
  if (!wait_ || !prop || prop->window != x_window_ ||
      prop->atom != x11::GetAtom(property_) || ShouldKeepOnWaiting()) {
    return;
  }

  wait_ = false;
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

}  // namespace ui
