// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_
#define UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/x/event.h"

namespace ui {

// Blocks till the value of |property| on |window| changes.
class X11PropertyChangeWaiter : public XEventDispatcher {
 public:
  X11PropertyChangeWaiter(x11::Window window, const char* property);
  ~X11PropertyChangeWaiter() override;

  // Blocks till the value of |property_| changes.
  virtual void Wait();

 protected:
  // Returns whether the run loop can exit.
  virtual bool ShouldKeepOnWaiting(x11::Event* event);

  x11::Window xwindow() const { return x_window_; }

 private:
  // XEventDispatcher:
  bool DispatchXEvent(x11::Event* event) override;

  x11::Window x_window_;
  const char* property_;

  std::unique_ptr<XScopedEventSelector> x_window_events_;

  // Whether Wait() should block.
  bool wait_;

  // Ends the run loop.
  base::OnceClosure quit_closure_;

  std::unique_ptr<ScopedXEventDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(X11PropertyChangeWaiter);
};

}  // namespace ui

#endif  // UI_BASE_X_TEST_X11_PROPERTY_CHANGE_WAITER_H_
