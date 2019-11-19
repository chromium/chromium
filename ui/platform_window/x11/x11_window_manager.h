// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class X11Window;

class X11_WINDOW_EXPORT X11WindowManager {
 public:
  X11WindowManager();
  ~X11WindowManager();

  // Returns instance of X11WindowManager.
  static X11WindowManager* GetInstance();

  // Sets a given X11Window as the recipient for events and calls
  // OnLostCapture for another |located_events_grabber_| if it has been set
  // previously.
  void GrabEvents(X11Window* window);

  // Unsets a given X11Window as the recipient for events and calls
  // OnLostCapture.
  void UngrabEvents(X11Window* window);

  // Gets the current X11PlatformWindow recipient of mouse events.
  X11Window* located_events_grabber() const { return located_events_grabber_; }

  // Gets the window corresponding to the AcceleratedWidget |widget|.
  void AddWindow(X11Window* window);
  void RemoveWindow(X11Window* window);
  X11Window* GetWindow(gfx::AcceleratedWidget widget) const;

  void MouseOnWindow(X11Window* delegate);

  const X11Window* window_mouse_currently_on_for_test() const {
    return window_mouse_currently_on_;
  }

 private:
  X11Window* located_events_grabber_ = nullptr;
  X11Window* window_mouse_currently_on_ = nullptr;

  base::flat_map<gfx::AcceleratedWidget, X11Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowManager);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_H_
