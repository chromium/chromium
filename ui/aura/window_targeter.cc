// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"

namespace aura {

WindowTargeter::WindowTargeter() = default;

WindowTargeter::~WindowTargeter() = default;

bool WindowTargeter::SubtreeShouldBeExploredForEvent(
    Window* window,
    const ui::LocatedEvent& event) {
  return SubtreeCanAcceptEvent(window, event) &&
         EventLocationInsideBounds(window, event);
}

bool WindowTargeter::GetHitTestRects(Window* window,
                                     gfx::Rect* hit_test_rect_mouse,
                                     gfx::Rect* hit_test_rect_touch) const {
  DCHECK(hit_test_rect_mouse);
  DCHECK(hit_test_rect_touch);
  *hit_test_rect_mouse = *hit_test_rect_touch = window->bounds();

  if (ShouldUseExtendedBounds(window)) {
    hit_test_rect_mouse->Inset(mouse_extend_);
    hit_test_rect_touch->Inset(touch_extend_);
  }

  return true;
}

std::unique_ptr<WindowTargeter::HitTestRects>
WindowTargeter::GetExtraHitTestShapeRects(Window* target) const {
  return nullptr;
}

void WindowTargeter::SetInsets(const gfx::Insets& mouse_and_touch_extend) {
  SetInsets(mouse_and_touch_extend, mouse_and_touch_extend);
}

void WindowTargeter::SetInsets(const gfx::Insets& mouse_extend,
                               const gfx::Insets& touch_extend) {
  if (mouse_extend_ == mouse_extend && touch_extend_ == touch_extend)
    return;

  mouse_extend_ = mouse_extend;
  touch_extend_ = touch_extend;
}

Window* WindowTargeter::GetPriorityTargetInRootWindow(
    Window* root_window,
    const ui::LocatedEvent& event) {
  DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Mouse events should be dispatched to the window that processed the
  // mouse-press events (if any).
  if (event.IsScrollEvent() || event.IsMouseEvent()) {
    WindowEventDispatcher* dispatcher = root_window->GetHost()->dispatcher();
    if (dispatcher->mouse_pressed_handler())
      return dispatcher->mouse_pressed_handler();
  }

  // All events should be directed towards the capture window (if any).
  Window* capture_window = client::GetCaptureWindow(root_window);
  if (capture_window)
    return capture_window;

  if (event.IsPinchEvent()) {
    DCHECK_EQ(event.AsGestureEvent()->details().device_type(),
              ui::GestureDeviceType::DEVICE_TOUCHPAD);
    WindowEventDispatcher* dispatcher = root_window->GetHost()->dispatcher();
    if (dispatcher->touchpad_pinch_handler())
      return dispatcher->touchpad_pinch_handler();
  }

  if (event.IsTouchEvent()) {
    // Query the gesture-recognizer to find targets for touch events.
    const ui::TouchEvent& touch = *event.AsTouchEvent();
    ui::GestureConsumer* consumer =
        Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(touch);
    if (consumer)
      return static_cast<Window*>(consumer);
  }

  return nullptr;
}

Window* WindowTargeter::FindTargetInRootWindow(Window* root_window,
                                               const ui::LocatedEvent& event) {
  DCHECK_EQ(root_window, root_window->GetRootWindow());

  Window* priority_target = GetPriorityTargetInRootWindow(root_window, event);
  if (priority_target)
    return priority_target;

  if (event.IsTouchEvent()) {
    // Query the gesture-recognizer to find targets for touch events.
    const ui::TouchEvent& touch = *event.AsTouchEvent();
    // GetTouchLockedTarget() is handled in GetPriorityTargetInRootWindow().
    ui::GestureRecognizer* gesture_recognizer =
        Env::GetInstance()->gesture_recognizer();
    DCHECK(!gesture_recognizer->GetTouchLockedTarget(touch));
    ui::GestureConsumer* consumer = gesture_recognizer->GetTargetForLocation(
        event.location_f(), touch.source_device_id());
    if (consumer)
      return static_cast<Window*>(consumer);

#if defined(OS_CHROMEOS)
    // If the initial touch is outside the window's display, target the root.
    // This is used for bezel gesture events (eg. swiping in from screen edge).
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
    // The window target may be null, so use the root's ScreenPositionClient.
    gfx::Point screen_location = event.root_location();
    if (client::GetScreenPositionClient(root_window)) {
      client::GetScreenPositionClient(root_window)
          ->ConvertPointToScreen(root_window, &screen_location);
    }
    if (!display.bounds().Contains(screen_location))
      return root_window;
#else
    // If the initial touch is outside the root window, target the root.
    // TODO: this code is likely not necessarily and will be removed.
    if (!root_window->bounds().Contains(event.location()))
      return root_window;
#endif
  }

  return nullptr;
}

bool WindowTargeter::ProcessEventIfTargetsDifferentRootWindow(
    Window* root_window,
    Window* target,
    ui::Event* event) {
  if (root_window->Contains(target))
    return false;

  // |window| is the root window, but |target| is not a descendent of
  // |window|. So do not allow dispatching from here. Instead, dispatch the
  // event through the WindowEventDispatcher that owns |target|.
  Window* new_root = target->GetRootWindow();
  DCHECK(new_root);
  if (event->IsLocatedEvent()) {
    // The event has been transformed to be in |target|'s coordinate system.
    // But dispatching the event through the EventProcessor requires the event
    // to be in the host's coordinate system. So, convert the event to be in
    // the root's coordinate space, and then to the host's coordinate space by
    // applying the host's transform.
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    located_event->ConvertLocationToTarget(target, new_root);
    WindowTreeHost* window_tree_host = new_root->GetHost();
    located_event->UpdateForRootTransform(
        window_tree_host->GetRootTransform(),
        window_tree_host->GetRootTransformForLocalEventCoordinates());
  }
  ignore_result(new_root->GetHost()->event_sink()->OnEventFromSource(event));
  return true;
}

ui::EventTarget* WindowTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                    ui::Event* event) {
  Window* window = static_cast<Window*>(root);
  Window* target = event->IsKeyEvent()
                       ? FindTargetForKeyEvent(window)
                       : FindTargetForNonKeyEvent(window, event);
  if (target && !window->parent() &&
      ProcessEventIfTargetsDifferentRootWindow(window, target, event)) {
    return nullptr;
  }
  return target;
}

ui::EventTarget* WindowTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return nullptr;
}

Window* WindowTargeter::FindTargetForKeyEvent(Window* window) {
  Window* root_window = window->GetRootWindow();
  client::FocusClient* focus_client = client::GetFocusClient(root_window);
  if (!focus_client)
    return window;
  Window* focused_window = focus_client->GetFocusedWindow();
  if (!focused_window)
    return window;

  client::EventClient* event_client = client::GetEventClient(root_window);
  if (event_client &&
      !event_client->CanProcessEventsWithinSubtree(focused_window)) {
    focus_client->FocusWindow(nullptr);
    return nullptr;
  }
  return focused_window ? focused_window : window;
}

void WindowTargeter::OnInstalled(Window* window) {
  window_ = window;
}

Window* WindowTargeter::FindTargetForLocatedEvent(Window* window,
                                                  ui::LocatedEvent* event) {
  if (!window->parent()) {
    Window* target = FindTargetInRootWindow(window, *event);
    if (target) {
      window->ConvertEventToTarget(target, event);

#if defined(OS_CHROMEOS)
      if (window->IsRootWindow() && event->HasNativeEvent()) {
        // If window is root, and the target is in a different host, we need to
        // convert the native event to the target's host as well. This happens
        // while a widget is being dragged and when the majority of its bounds
        // reside in a different display. Setting the widget's bounds at this
        // point changes the window's root, and the event's target's root, but
        // the events are still being generated relative to the original
        // display. crbug.com/714578.
        ui::LocatedEvent* e =
            static_cast<ui::LocatedEvent*>(event->native_event());
        gfx::PointF native_point = e->location_f();
        aura::Window::ConvertNativePointToTargetHost(window, target,
                                                     &native_point);
        e->set_location_f(native_point);
      }
#endif

      return target;
    }
  }
  return FindTargetForLocatedEventRecursively(window, event);
}

bool WindowTargeter::SubtreeCanAcceptEvent(
    Window* window,
    const ui::LocatedEvent& event) const {
  if (!window->IsVisible())
    return false;
  if (window->event_targeting_policy() == EventTargetingPolicy::kNone ||
      window->event_targeting_policy() == EventTargetingPolicy::kTargetOnly) {
    return false;
  }
  client::EventClient* client = client::GetEventClient(window->GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(window))
    return false;

  Window* parent = window->parent();
  if (parent && parent->delegate_ &&
      !parent->delegate_->ShouldDescendIntoChildForEventHandling(
          window, event.location())) {
    return false;
  }
  return true;
}

bool WindowTargeter::EventLocationInsideBounds(
    Window* window,
    const ui::LocatedEvent& event) const {
  gfx::Rect mouse_rect;
  gfx::Rect touch_rect;
  if (!GetHitTestRects(window, &mouse_rect, &touch_rect))
    return false;

  const gfx::Vector2d offset = -window->bounds().OffsetFromOrigin();
  mouse_rect.Offset(offset);
  touch_rect.Offset(offset);
  gfx::Point point = event.location();
  if (window->parent())
    Window::ConvertPointToTarget(window->parent(), window, &point);

  const bool point_in_rect = event.IsTouchEvent() || event.IsGestureEvent()
                                 ? touch_rect.Contains(point)
                                 : mouse_rect.Contains(point);
  if (!point_in_rect)
    return false;

  auto shape_rects = GetExtraHitTestShapeRects(window);
  if (!shape_rects)
    return true;

  for (const gfx::Rect& shape_rect : *shape_rects) {
    if (shape_rect.Contains(point)) {
      return true;
    }
  }

  return false;
}

bool WindowTargeter::ShouldUseExtendedBounds(const aura::Window* w) const {
  // window() is null when this is used as the default targeter (by
  // WindowEventDispatcher). Insets should never be set in this case, so the
  // return should not matter.
  if (!window()) {
    DCHECK(mouse_extend_.IsEmpty());
    DCHECK(touch_extend_.IsEmpty());
    return false;
  }

  // Insets should only apply to the window. Subclasses may enforce other
  // policies.
  return window() == w;
}

Window* WindowTargeter::FindTargetForNonKeyEvent(Window* root_window,
                                                 ui::Event* event) {
  if (!event->IsLocatedEvent())
    return root_window;
  return FindTargetForLocatedEvent(root_window,
                                   static_cast<ui::LocatedEvent*>(event));
}

Window* WindowTargeter::FindTargetForLocatedEventRecursively(
    Window* root_window,
    ui::LocatedEvent* event) {
  std::unique_ptr<ui::EventTargetIterator> iter =
      root_window->GetChildIterator();
  if (iter) {
    ui::EventTarget* target = root_window;
    for (ui::EventTarget* child = iter->GetNextTarget(); child;
         child = iter->GetNextTarget()) {
      WindowTargeter* targeter =
          static_cast<WindowTargeter*>(child->GetEventTargeter());
      if (!targeter)
        targeter = this;
      if (!targeter->SubtreeShouldBeExploredForEvent(
              static_cast<Window*>(child), *event)) {
        continue;
      }
      target->ConvertEventToTarget(child, event);
      target = child;
      Window* child_target_window =
          static_cast<Window*>(targeter->FindTargetForEvent(child, event));
      if (child_target_window)
        return child_target_window;
    }
    target->ConvertEventToTarget(root_window, event);
  }
  return root_window->CanAcceptEvent(*event) ? root_window : nullptr;
}

}  // namespace aura
