// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_event_filter_lacros.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#include "ui/views/widget/widget.h"

namespace views {

WindowEventFilterLacros::WindowEventFilterLacros(
    DesktopWindowTreeHostPlatform* desktop_window_tree_host,
    ui::WmMoveResizeHandler* handler)
    : desktop_window_tree_host_(desktop_window_tree_host), handler_(handler) {
  desktop_window_tree_host_->window()->AddPreTargetHandler(this);
}

WindowEventFilterLacros::~WindowEventFilterLacros() {
  desktop_window_tree_host_->window()->RemovePreTargetHandler(this);
}

void WindowEventFilterLacros::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed ||
      !event->IsOnlyLeftMouseButton()) {
    return;
  }

  auto* window = static_cast<aura::Window*>(event->target());
  int component =
      window->delegate()
          ? window->delegate()->GetNonClientComponent(event->location())
          : HTNOWHERE;

  if (event->flags() & ui::EF_IS_DOUBLE_CLICK) {
    if (previous_pressed_component_ == HTCAPTION && component == HTCAPTION) {
      MaybeToggleMaximizedState(window);
      previous_pressed_component_ = HTNOWHERE;
      event->SetHandled();
    }
    return;
  }
  previous_pressed_component_ = component;
  MaybeDispatchHostWindowDragMovement(component, event);
}

void WindowEventFilterLacros::OnGestureEvent(ui::GestureEvent* event) {
  auto* window = static_cast<aura::Window*>(event->target());
  int component =
      window->delegate()
          ? window->delegate()->GetNonClientComponent(event->location())
          : HTNOWHERE;

  // Double tap to maximize.
  if (event->type() == ui::EventType::kGestureTap) {
    int previous_pressed_component = previous_pressed_component_;
    previous_pressed_component_ = component;

    if (previous_pressed_component_ == HTCAPTION &&
        previous_pressed_component == HTCAPTION &&
        event->details().tap_count() == 2) {
      MaybeToggleMaximizedState(window);
      previous_pressed_component_ = HTNOWHERE;
      event->SetHandled();
    }
    return;
  }

  // Interactive window move.
  if (event->type() == ui::EventType::kGestureScrollBegin) {
    MaybeDispatchHostWindowDragMovement(component, event);
  }
}

void WindowEventFilterLacros::MaybeToggleMaximizedState(aura::Window* window) {
  if (!(window->GetProperty(aura::client::kResizeBehaviorKey) &
        aura::client::kResizeBehaviorCanMaximize)) {
    return;
  }

  // TODO(crbug.com/40215883): send toggle event to ash-chrome.
  if (desktop_window_tree_host_->IsMaximized())
    desktop_window_tree_host_->Restore();
  else
    desktop_window_tree_host_->Maximize();
}

void WindowEventFilterLacros::MaybeDispatchHostWindowDragMovement(
    int component,
    ui::LocatedEvent* event) {
  if (!handler_ || !ui::CanPerformDragOrResize(component))
    return;

  // The location argument is not used in lacros as the drag and resize
  // are handled by the compositor (ash-chrome).
  handler_->DispatchHostWindowDragMovement(component, gfx::Point());

  // Stop the event propagation for mouse events only (not touch), given that
  // it'd prevent the Gesture{Provider,Detector} machirery to get triggered,
  // breaking gestures including tapping, double tapping, show press and
  // long press.
  if (event->IsMouseEvent())
    event->StopPropagation();
}

}  // namespace views
