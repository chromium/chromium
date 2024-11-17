// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class X11Window;

class X11WindowManager {
 public:
  X11WindowManager();

  X11WindowManager(const X11WindowManager&) = delete;
  X11WindowManager& operator=(const X11WindowManager&) = delete;

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
  raw_ptr<X11Window> located_events_grabber_ = nullptr;
  raw_ptr<X11Window> window_mouse_currently_on_ = nullptr;

  base::flat_map<gfx::AcceleratedWidget, raw_ptr<X11Window, CtnExperimental>>
      windows_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_
