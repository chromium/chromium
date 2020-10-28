// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_MENU_REGISTRAR_H_
#define UI_BASE_X_X11_MENU_REGISTRAR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/event.h"

namespace ui {

class XScopedEventSelector;

// A singleton that owns global objects related to the desktop and listens for
// X11 events on the X11 root window. Destroys itself when the browser
// shuts down.
class X11MenuRegistrar : public ui::XEventDispatcher {
 public:
  // Returns the singleton handler.  Creates one if one has not
  // already been created.
  static X11MenuRegistrar* Get();

  // ui::XEventDispatcher
  bool DispatchXEvent(x11::Event* event) override;

 private:
  X11MenuRegistrar();
  ~X11MenuRegistrar() override;

  // Called when |window| has been created or destroyed. |window| may not be
  // managed by Chrome.
  void OnWindowCreatedOrDestroyed(bool created, x11::Window window);

  // Events selected on |x_root_window_|.
  std::unique_ptr<ui::XScopedEventSelector> x_root_window_events_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_MENU_REGISTRAR_H_
