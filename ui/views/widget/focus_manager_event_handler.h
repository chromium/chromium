// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_FOCUS_MANAGER_EVENT_HANDLER_H_
#define UI_VIEWS_WIDGET_FOCUS_MANAGER_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace views {

class Widget;

// This class forwards KeyEvents to the FocusManager associated with a widget.
// This allows KeyEvents to be processed before other targets.
class FocusManagerEventHandler : public ui::EventHandler {
 public:
  FocusManagerEventHandler(Widget* widget, aura::Window* window);
  ~FocusManagerEventHandler() override;

  // Implementation of ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  Widget* widget_;

  // |window_| is the event target that is associated with this class.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerEventHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_FOCUS_MANAGER_EVENT_HANDLER_H_
