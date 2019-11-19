// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/x11_property_change_waiter.h"

#include <utility>

#include "base/run_loop.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace views {

X11PropertyChangeWaiter::X11PropertyChangeWaiter(XID window,
                                                 const char* property)
    : x_window_(window), property_(property), wait_(true) {
  // Ensure that we are listening to PropertyNotify events for |window|. This
  // is not the case for windows which were not created by
  // DesktopWindowTreeHostX11.
  x_window_events_ =
      std::make_unique<ui::XScopedEventSelector>(x_window_, PropertyChangeMask);

  // Override the dispatcher so that we get events before
  // DesktopWindowTreeHostX11 does. We must do this because
  // DesktopWindowTreeHostX11 stops propagation.
  dispatcher_ =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);
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

bool X11PropertyChangeWaiter::ShouldKeepOnWaiting(
    const ui::PlatformEvent& event) {
  // Stop waiting once we get a property change.
  return true;
}

bool X11PropertyChangeWaiter::CanDispatchEvent(const ui::PlatformEvent& event) {
  NOTREACHED();
  return true;
}

uint32_t X11PropertyChangeWaiter::DispatchEvent(
    const ui::PlatformEvent& event) {
  if (!wait_ || event->type != PropertyNotify ||
      event->xproperty.window != x_window_ ||
      event->xproperty.atom != gfx::GetAtom(property_) ||
      ShouldKeepOnWaiting(event)) {
    return ui::POST_DISPATCH_PERFORM_DEFAULT;
  }

  wait_ = false;
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
  return ui::POST_DISPATCH_PERFORM_DEFAULT;
}

}  // namespace views
