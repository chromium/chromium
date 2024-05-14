// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/events_ozone.h"

#include "base/check.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ui {
namespace {
bool dispatch_disabled = false;
}

EventResult DispatchEventFromNativeUiEvent(
    const PlatformEvent& event,
    base::OnceCallback<void(ui::Event*)> callback) {
  // If it's disabled for test, just return as if it's handled.
  if (dispatch_disabled)
    return ER_HANDLED;

  // NB: ui::Events are constructed here using the overload that takes a
  // const PlatformEvent& (here ui::Event* const&) rather than the copy
  // constructor. This has side effects and cannot be changed to use the
  // copy constructor or Event::Clone.
  if (event->IsKeyEvent()) {
    ui::KeyEvent key_event(event);
    std::move(callback).Run(&key_event);
    return key_event.result();
  } else if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent wheel_event(event);
    std::move(callback).Run(&wheel_event);
    return wheel_event.result();
  } else if (event->IsMouseEvent()) {
    ui::MouseEvent mouse_event(event);
    std::move(callback).Run(&mouse_event);
    return mouse_event.result();
  } else if (event->IsTouchEvent()) {
    ui::TouchEvent touch_event(event);
    std::move(callback).Run(&touch_event);
    return touch_event.result();
  } else if (event->IsScrollEvent()) {
    ui::ScrollEvent scroll_event(event);
    std::move(callback).Run(&scroll_event);
    return scroll_event.result();
  } else if (event->IsGestureEvent()) {
    // TODO(https://crbug.com/1355835: Missing ui::GestureEvent(const
    // PlatformEvent&) ctor).
    auto gesture_event = *(event->AsGestureEvent());
    std::move(callback).Run(&gesture_event);
    return gesture_event.result();
    // TODO(mohsen): Use the same pattern for scroll/touch/wheel events.
    // Apparently, there is no need for them to wrap the |event|.
  } else {
    NOTREACHED_IN_MIGRATION();
    return ER_UNHANDLED;
  }
}

void DisableNativeUiEventDispatchForTest() {
  dispatch_disabled = true;
}

bool IsNativeUiEventDispatchDisabled() {
  return dispatch_disabled;
}

void SetKeyboardImeFlagProperty(KeyEvent::Properties* properties,
                                uint8_t flags) {
  properties->emplace(kPropertyKeyboardImeFlag, std::vector<uint8_t>{flags});
}

void SetKeyboardImeFlags(KeyEvent* event, uint8_t flags) {
  Event::Properties properties;
  if (const auto* original = event->properties()) {
    properties = *original;
  }
  SetKeyboardImeFlagProperty(&properties, flags);
  event->SetProperties(properties);
}

uint8_t GetKeyboardImeFlags(const KeyEvent& event) {
  const auto* properties = event.properties();
  if (!properties) {
    return 0;
  }
  auto it = properties->find(kPropertyKeyboardImeFlag);
  if (it == properties->end()) {
    return 0;
  }
  DCHECK_EQ(1u, it->second.size());
  return it->second[0];
}

}  // namespace ui
