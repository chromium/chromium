// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/events_ozone.h"

#include "ui/events/event.h"

namespace ui {
namespace {
bool dispatch_disabled = false;
}

bool DispatchEventFromNativeUiEvent(
    const PlatformEvent& event,
    base::OnceCallback<void(ui::Event*)> callback) {
  // If it's disabled for test, just return as if it's handled.
  if (dispatch_disabled)
    return true;

  // NB: ui::Events are constructed here using the overload that takes a
  // const PlatformEvent& (here ui::Event* const&) rather than the copy
  // constructor. This has side effects and cannot be changed to use the
  // copy constructor or Event::Clone.
  bool handled = false;
  if (event->IsKeyEvent()) {
    ui::KeyEvent key_event(event);
    std::move(callback).Run(&key_event);
    handled = key_event.handled();
  } else if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent wheel_event(event);
    std::move(callback).Run(&wheel_event);
    handled = wheel_event.handled();
  } else if (event->IsMouseEvent()) {
    ui::MouseEvent mouse_event(event);
    std::move(callback).Run(&mouse_event);
    handled = mouse_event.handled();
  } else if (event->IsTouchEvent()) {
    ui::TouchEvent touch_event(event);
    std::move(callback).Run(&touch_event);
    handled = touch_event.handled();
  } else if (event->IsScrollEvent()) {
    ui::ScrollEvent scroll_event(event);
    std::move(callback).Run(&scroll_event);
    handled = scroll_event.handled();
  } else if (event->IsGestureEvent()) {
    // TODO(https://crbug.com/1355835: Missing ui::GestureEvent(const
    // PlatformEvent&) ctor).
    auto gesture_event = *(event->AsGestureEvent());
    std::move(callback).Run(&gesture_event);
    handled = gesture_event.handled();
    // TODO(mohsen): Use the same pattern for scroll/touch/wheel events.
    // Apparently, there is no need for them to wrap the |event|.
  } else {
    NOTREACHED();
  }

  return handled;
}

EVENTS_EXPORT void DisableNativeUiEventDispatchForTest() {
  dispatch_disabled = true;
}

}  // namespace ui
