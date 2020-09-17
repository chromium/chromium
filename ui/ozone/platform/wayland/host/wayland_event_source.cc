// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

bool HasAnyPointerButtonFlag(int flags) {
  return (flags & (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
                   EF_RIGHT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON |
                   EF_FORWARD_MOUSE_BUTTON)) != 0;
}

}  // namespace

struct WaylandEventSource::TouchPoint {
  TouchPoint(gfx::PointF location, WaylandWindow* current_window);
  ~TouchPoint() = default;

  WaylandWindow* const window;
  gfx::PointF last_known_location;
};

WaylandEventSource::TouchPoint::TouchPoint(gfx::PointF location,
                                           WaylandWindow* current_window)
    : window(current_window), last_known_location(location) {
  DCHECK(window);
}

// WaylandEventSource implementation

WaylandEventSource::WaylandEventSource(wl_display* display,
                                       WaylandWindowManager* window_manager)
    : window_manager_(window_manager),
      event_watcher_(std::make_unique<WaylandEventWatcher>(display)) {
  DCHECK(window_manager_);

  // Observes remove changes to know when touch points can be removed.
  window_manager_->AddObserver(this);
}

WaylandEventSource::~WaylandEventSource() = default;

bool WaylandEventSource::StartProcessingEvents() {
  return event_watcher_->StartProcessingEvents();
}

bool WaylandEventSource::StopProcessingEvents() {
  return event_watcher_->StopProcessingEvents();
}

void WaylandEventSource::OnKeyboardCreated(WaylandKeyboard* keyboard) {
  DCHECK(keyboard);
  keyboard_ = keyboard;
}

void WaylandEventSource::OnKeyboardDestroyed(WaylandKeyboard* keyboard) {
  DCHECK_EQ(keyboard_, keyboard);
  keyboard_modifiers_ = 0;
  keyboard_ = nullptr;
}

void WaylandEventSource::OnKeyboardFocusChanged(WaylandWindow* window,
                                                bool focused) {
  DCHECK(window);
  HandleKeyboardFocusChange(window, focused);
}

void WaylandEventSource::OnKeyboardModifiersChanged(int modifiers) {
  keyboard_modifiers_ = modifiers;
}

uint32_t WaylandEventSource::OnKeyboardKeyEvent(EventType type,
                                                DomCode dom_code,
                                                DomKey dom_key,
                                                KeyboardCode key_code,
                                                bool repeat,
                                                base::TimeTicks timestamp) {
  DCHECK(type == ET_KEY_PRESSED || type == ET_KEY_RELEASED);
  if (!keyboard_)
    return POST_DISPATCH_NONE;

  // try to decode key, if not yet.
  if (dom_key == DomKey::NONE &&
      !keyboard_->Decode(dom_code, keyboard_modifiers_, &dom_key, &key_code)) {
    LOG(ERROR) << "Failed to decode key event.";
    return POST_DISPATCH_NONE;
  }

  if (!repeat) {
    int flag = ModifierDomKeyToEventFlag(dom_key);
    UpdateKeyboardModifiers(flag, type == ET_KEY_PRESSED);
  }

  KeyEvent event(type, key_code, dom_code, keyboard_modifiers_, dom_key,
                 timestamp);
  event.set_source_device_id(keyboard_->device_id());
  return DispatchEvent(&event);
}

void WaylandEventSource::OnPointerCreated(WaylandPointer* pointer) {
  DCHECK(pointer);
  pointer_ = pointer;
}

void WaylandEventSource::OnPointerDestroyed(WaylandPointer* pointer) {
  DCHECK_EQ(pointer_, pointer);

  // Clear focused window, if any.
  HandlePointerFocusChange(nullptr);

  ResetPointerFlags();
  pointer_ = nullptr;
}

void WaylandEventSource::OnPointerFocusChanged(WaylandWindow* window,
                                               const gfx::PointF& location) {
  if (!pointer_)
    return;

  // Save new pointer location.
  pointer_location_ = location;

  bool focused = !!window;
  if (focused)
    HandlePointerFocusChange(window);

  EventType type = focused ? ET_MOUSE_ENTERED : ET_MOUSE_EXITED;
  MouseEvent event(type, location, location, EventTimeForNow(), pointer_flags_,
                   0);
  DispatchEvent(&event);

  if (!focused)
    HandlePointerFocusChange(nullptr);
}

void WaylandEventSource::OnPointerButtonEvent(EventType type,
                                              int changed_button,
                                              WaylandWindow* window) {
  DCHECK(type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED);
  DCHECK(HasAnyPointerButtonFlag(changed_button));

  if (!pointer_)
    return;

  auto* prev_focused_window = window_with_pointer_focus_;
  if (window)
    HandlePointerFocusChange(window);

  pointer_flags_ = type == ET_MOUSE_PRESSED
                       ? (pointer_flags_ | changed_button)
                       : (pointer_flags_ & ~changed_button);
  last_pointer_button_pressed_ = changed_button;
  // MouseEvent's flags should contain the button that was released too.
  int flags = pointer_flags_ | keyboard_modifiers_ | changed_button;
  MouseEvent event(type, pointer_location_, pointer_location_,
                   EventTimeForNow(), flags, changed_button);
  DispatchEvent(&event);

  if (window)
    HandlePointerFocusChange(prev_focused_window);
}

void WaylandEventSource::OnPointerMotionEvent(const gfx::PointF& location) {
  pointer_location_ = location;
  int flags = pointer_flags_ | keyboard_modifiers_;
  MouseEvent event(ET_MOUSE_MOVED, pointer_location_, pointer_location_,
                   EventTimeForNow(), flags, 0);
  DispatchEvent(&event);
}

void WaylandEventSource::OnPointerAxisEvent(const gfx::Vector2d& offset) {
  int flags = pointer_flags_ | keyboard_modifiers_;
  MouseWheelEvent event(offset, pointer_location_, pointer_location_,
                        EventTimeForNow(), flags, 0);
  DispatchEvent(&event);
}

void WaylandEventSource::OnTouchCreated(WaylandTouch* touch) {
  DCHECK(touch);
  touch_ = touch;
}

void WaylandEventSource::OnTouchDestroyed(WaylandTouch* touch) {
  DCHECK_EQ(touch_, touch);
  touch_points_.clear();
  touch_ = nullptr;
}

void WaylandEventSource::OnTouchPressEvent(WaylandWindow* window,
                                           const gfx::PointF& location,
                                           base::TimeTicks timestamp,
                                           PointerId id) {
  DCHECK(window);
  HandleTouchFocusChange(window, true);

  // Make sure this touch point wasn't present before.
  auto success = touch_points_.try_emplace(
      id, std::make_unique<TouchPoint>(location, window));
  if (!success.second) {
    LOG(WARNING) << "Touch down fired with wrong id";
    return;
  }

  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_PRESSED, location, location, timestamp, details);
  DispatchEvent(&event);
}

void WaylandEventSource::OnTouchReleaseEvent(base::TimeTicks timestamp,
                                             PointerId id) {
  // Make sure this touch point was present before.
  const auto it = touch_points_.find(id);
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch up fired with no matching touch down";
    return;
  }

  TouchPoint* touch_point = it->second.get();
  gfx::PointF location = touch_point->last_known_location;
  PointerDetails details(EventPointerType::kTouch, id);

  TouchEvent event(ET_TOUCH_RELEASED, location, location, timestamp, details);
  DispatchEvent(&event);

  HandleTouchFocusChange(touch_point->window, false, id);
  touch_points_.erase(it);
}

void WaylandEventSource::OnTouchMotionEvent(const gfx::PointF& location,
                                            base::TimeTicks timestamp,
                                            PointerId id) {
  const auto it = touch_points_.find(id);
  // Make sure this touch point was present before.
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }
  it->second->last_known_location = location;
  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_MOVED, location, location, timestamp, details);
  DispatchEvent(&event);
}

void WaylandEventSource::OnTouchCancelEvent() {
  gfx::PointF location;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (auto& touch_point : touch_points_) {
    PointerId id = touch_point.first;
    TouchEvent event(ET_TOUCH_CANCELLED, location, location, timestamp,
                     PointerDetails(EventPointerType::kTouch, id));
    DispatchEvent(&event);
    HandleTouchFocusChange(touch_point.second->window, false);
  }
  touch_points_.clear();
}

bool WaylandEventSource::IsPointerButtonPressed(EventFlags button) const {
  DCHECK(HasAnyPointerButtonFlag(button));
  return pointer_flags_ & button;
}

void WaylandEventSource::ResetPointerFlags() {
  pointer_flags_ = 0;
}

void WaylandEventSource::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandEventSource::OnWindowRemoved(WaylandWindow* window) {
  // Clear pointer-related data.
  if (window == window_with_pointer_focus_)
    window_with_pointer_focus_ = nullptr;

  // Clear touch-related data.
  base::EraseIf(touch_points_, [window](const auto& point) {
    return point.second->window == window;
  });
}

// Currently EF_MOD3_DOWN means that the CapsLock key is currently down, and
// EF_CAPS_LOCK_ON means the caps lock state is enabled (and the key may or
// may not be down, but usually isn't). There does need to be two different
// flags, since the physical CapsLock key is subject to remapping, but the
// caps lock state (which can be triggered in a variety of ways) is not.
//
// TODO(crbug.com/1076661): This is likely caused by some CrOS-specific code.
// Get rid of this function once it is properly guarded under OS_CHROMEOS.
void WaylandEventSource::UpdateKeyboardModifiers(int modifier, bool down) {
  if (modifier == EF_NONE)
    return;

  if (modifier == EF_CAPS_LOCK_ON) {
    modifier = (modifier & ~EF_CAPS_LOCK_ON) | EF_MOD3_DOWN;
  }
  keyboard_modifiers_ = down ? (keyboard_modifiers_ | modifier)
                             : (keyboard_modifiers_ & ~modifier);
}

void WaylandEventSource::HandleKeyboardFocusChange(WaylandWindow* window,
                                                   bool focused) {
  DCHECK(window);
  window->set_keyboard_focus(focused);
}

void WaylandEventSource::HandlePointerFocusChange(WaylandWindow* window) {
  // Focused window might have been destroyed at this point (eg: context menus),
  // in this case, |window| is null.
  if (window_with_pointer_focus_)
    window_with_pointer_focus_->SetPointerFocus(false);

  window_with_pointer_focus_ = window;

  if (window_with_pointer_focus_)
    window_with_pointer_focus_->SetPointerFocus(true);
}

void WaylandEventSource::HandleTouchFocusChange(WaylandWindow* window,
                                                bool focused,
                                                base::Optional<PointerId> id) {
  DCHECK(window);
  bool actual_focus = id ? !ShouldUnsetTouchFocus(window, id.value()) : focused;
  window->set_touch_focus(actual_focus);
}

// Focus must not be unset if there is another touch point within |window|.
bool WaylandEventSource::ShouldUnsetTouchFocus(WaylandWindow* win,
                                               PointerId id) {
  auto result = std::find_if(
      touch_points_.begin(), touch_points_.end(),
      [win, id](auto& p) { return p.second->window == win && p.first != id; });
  return result == touch_points_.end();
}

}  // namespace ui
