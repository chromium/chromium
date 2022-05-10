// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_WINDOW_BUTTON_ORDER_OBSERVER_H_
#define UI_VIEWS_LINUX_UI_WINDOW_BUTTON_ORDER_OBSERVER_H_

#include "ui/views/window/frame_buttons.h"

namespace views {

// Observer interface to receive the ordering of the min,max,close buttons.
class WindowButtonOrderObserver {
 public:
  // Called on a system-wide configuration event.
  virtual void OnWindowButtonOrderingChange() = 0;

 protected:
  virtual ~WindowButtonOrderObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_WINDOW_BUTTON_ORDER_OBSERVER_H_
