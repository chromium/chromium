// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_WINDOW_BUTTON_ORDER_OBSERVER_H_
#define UI_LINUX_WINDOW_BUTTON_ORDER_OBSERVER_H_

namespace ui {

// Observer interface to receive the ordering of the min,max,close buttons.
class WindowButtonOrderObserver {
 public:
  // Called on a system-wide configuration event.
  virtual void OnWindowButtonOrderingChange() = 0;

 protected:
  virtual ~WindowButtonOrderObserver() = default;
};

}  // namespace ui

#endif  // UI_LINUX_WINDOW_BUTTON_ORDER_OBSERVER_H_
