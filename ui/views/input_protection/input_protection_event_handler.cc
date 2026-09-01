// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_event_handler.h"

#include <cstdint>
#include <vector>

#include "base/check.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool HasInputProtectedProperty(const ui::Event& event) {
  const auto* properties = event.properties();
  return properties && properties->contains(kPropertyInputProtected);
}

void SetInputProtectedProperty(ui::Event& event) {
  event.SetProperty(kPropertyInputProtected, std::vector<uint8_t>());
}

// Returns true if the passed `key_event` is an arrow key navigation event
// (up, down, left, or right key press with no modifiers).
bool IsArrowTraversalKeyEvent(const ui::KeyEvent& key_event) {
  if (key_event.IsShiftDown() || key_event.IsControlDown() ||
      key_event.IsAltDown() || key_event.IsAltGrDown()) {
    return false;
  }

  const ui::KeyboardCode key = key_event.key_code();
  return key == ui::VKEY_UP || key == ui::VKEY_DOWN || key == ui::VKEY_LEFT ||
         key == ui::VKEY_RIGHT;
}

// Resolves the targeted View for an event. For key events, queries the
// `FocusManager` for the currently focused view; for pointer events, returns
// the event target.
View* GetEventTargetView(const ui::Event& event,
                         internal::RootView& root_view) {
  if (event.IsKeyEvent()) {
    FocusManager* focus_manager = root_view.GetFocusManager();
    return focus_manager ? focus_manager->GetFocusedView()
                         : static_cast<View*>(event.target());
  }
  return static_cast<View*>(event.target());
}

// Returns true if the passed `key_event` is any focus traversal event (Tab or
// Arrow key). If the focused view requests to skip default key event processing
// (e.g. Textfield or Button), this returns false for arrow keys.
bool IsFocusTraversalKeyEvent(const ui::KeyEvent& key_event,
                              internal::RootView& root_view) {
  if (FocusManager::IsTabTraversalKeyEvent(key_event)) {
    return true;
  }

  if (!IsArrowTraversalKeyEvent(key_event)) {
    return false;
  }

  View* focused_view = GetEventTargetView(key_event, root_view);
  if (focused_view && focused_view->SkipDefaultKeyEventProcessing(key_event)) {
    return false;
  }

  return true;
}

}  // namespace

InputProtectionEventHandler::InputProtectionEventHandler(
    internal::RootView* root_view)
    : root_view_(root_view) {
  CHECK(root_view_);
  root_view_->AddPreTargetHandler(this);
}

InputProtectionEventHandler::~InputProtectionEventHandler() {
  root_view_->RemovePreTargetHandler(this);
}

void InputProtectionEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Focus traversal keys (Tab, Shift-Tab, Arrow keys) should always be
  // permitted so users can navigate between views.
  if (IsFocusTraversalKeyEvent(*event, *root_view_)) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::EventType::kTouchPressed) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::EventType::kGestureTap &&
      type != ui::EventType::kGestureTapDown &&
      type != ui::EventType::kGestureDoubleTap &&
      type != ui::EventType::kGestureLongPress &&
      type != ui::EventType::kGestureLongTap) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::MaybeBlockEvent(ui::Event* event) {
  if (!event->cancelable()) {
    return;
  }

  // Do not block input events when accessibility is enabled.
  if (!ui::AXPlatform::GetInstance().GetMode().is_mode_off()) {
    return;
  }

  Widget* const widget = root_view_->GetWidget();
  if (!widget) {
    return;
  }

  // If this event has already been through the input protection framework,
  // skip it to prevent duplicate evaluations.
  if (HasInputProtectedProperty(*event)) {
    return;
  }

  SetInputProtectedProperty(*event);

  View* const target_view = GetEventTargetView(*event, *root_view_);

  // Check global occlusion protection:
  // - Located events (clicks and taps): Blocked if the event coordinate is
  //   occluded by an always-on-top window.
  // - Non-located events (keyboard actions like Space/Enter): Blocked if the
  //   target view (obtained using `GetEventTargetView()`) is occluded, but only
  //   if input activation protection is enabled on the widget.
  if (target_view &&
      OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
          *event, *target_view)) {
    event->StopPropagation();
    root_view_->ResetEventHandlers();
    return;
  }

  // Additional policies only apply if the widget explicitly opted into input
  // event activation protection.
  if (!widget->IsInputEventActivationProtectionEnabled()) {
    return;
  }

  // Evaluate whether any of the policies, installed on the widget input
  // protector, blocks the event.
  if (!widget->GetInputEventActivationProtector()
           ->IsPossiblyUnintendedInteraction(*event, /*allow_key_events=*/false,
                                             target_view)) {
    return;
  }

  // Discard the unintended event to prevent it from reaching the target view.
  event->StopPropagation();

  // Reset the RootView handler state so that any subsequent events from this
  // blocked interaction (e.g. mouse release) are discarded.
  root_view_->ResetEventHandlers();
}

}  // namespace views
