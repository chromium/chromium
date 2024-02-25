// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_
#define UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/window_event_manager.h"

namespace ui {

// Blocks till the value of |property| on |window| changes.
class X11PropertyChangeWaiter : public x11::EventObserver {
 public:
  X11PropertyChangeWaiter(x11::Window window, const char* property);

  X11PropertyChangeWaiter(const X11PropertyChangeWaiter&) = delete;
  X11PropertyChangeWaiter& operator=(const X11PropertyChangeWaiter&) = delete;

  ~X11PropertyChangeWaiter() override;

  // Blocks till the value of |property_| changes.
  virtual void Wait();

 protected:
  // Returns whether the run loop can exit.
  virtual bool ShouldKeepOnWaiting();

  x11::Window xwindow() const { return x_window_; }

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  const raw_ptr<x11::Connection> connection_;

  x11::Window x_window_;
  const char* property_;

  x11::ScopedEventSelector x_window_events_;

  // Whether Wait() should block.
  bool wait_;

  // Ends the run loop.
  base::OnceClosure quit_closure_;
};

}  // namespace ui

#endif  // UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_
