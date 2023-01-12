// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_X11_EVENT_WAITER_H_
#define UI_EVENTS_TEST_X11_EVENT_WAITER_H_

#include "base/functional/callback.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace ui {

// X11 Event Waiter class
class XEventWaiter : public x11::EventObserver {
 public:
  static XEventWaiter* Create(x11::Window window, base::OnceClosure callback);

 private:
  explicit XEventWaiter(base::OnceClosure callback);
  ~XEventWaiter() override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  // Returns atom that indidates that the XEvent is marker event.
  static x11::Atom MarkerEventAtom();

  base::OnceClosure success_callback_;
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_X11_EVENT_WAITER_H_
