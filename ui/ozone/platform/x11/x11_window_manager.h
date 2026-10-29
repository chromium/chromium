// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {

class TouchEvent;
class X11Window;

class X11WindowManager {
 public:
  // A touch sequence that was dispatched to an X11Window and has not ended.
  struct ActiveTouch {
    PointerId id;
    raw_ptr<X11Window> window;
    // The most recent location of the touch, relative to `window`, and its
    // root location.
    gfx::PointF location;
    gfx::PointF root_location;
  };

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

  // Returns true if pointer events are currently being delivered to Chrome,
  // i.e. the pointer is inside one of the windows or Chrome holds an active
  // pointer grab.  While this is true, X11EventSource::last_cursor_location()
  // is kept up to date by those events.
  bool IsTrackingPointer() const;

  // Returns true if any window has requested to be mapped but has not yet
  // received the corresponding MapNotify.
  bool HasWindowPendingMap() const;

  void MouseOnWindow(X11Window* delegate);

  // Records that `event` was dispatched to `window`, so that the touch
  // sequence can still be ended if its release never reaches any window (see
  // TakeActiveTouches()).
  void OnTouchEventDispatched(X11Window* window, const TouchEvent& event);

  // Removes and returns the active touch sequences whose ids are in `ids`.
  std::vector<ActiveTouch> TakeActiveTouches(const std::vector<PointerId>& ids);

  // Removes and returns all active touch sequences.
  std::vector<ActiveTouch> TakeAllActiveTouches();

  const X11Window* window_mouse_currently_on_for_test() const {
    return window_mouse_currently_on_;
  }

 private:
  raw_ptr<X11Window> located_events_grabber_ = nullptr;
  raw_ptr<X11Window> window_mouse_currently_on_ = nullptr;

  base::flat_map<gfx::AcceleratedWidget, raw_ptr<X11Window, CtnExperimental>>
      windows_;

  // Touch sequences that were dispatched to a window and have not ended, keyed
  // by id.
  base::flat_map<PointerId, ActiveTouch> active_touches_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_H_
