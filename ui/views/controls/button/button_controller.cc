// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button_controller.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller_delegate.h"

namespace views {

ButtonController::ButtonController(
    Button* button,
    std::unique_ptr<ButtonControllerDelegate> delegate)
    : button_(button), button_controller_delegate_(std::move(delegate)) {}

ButtonController::~ButtonController() = default;

bool ButtonController::OnMousePressed(const ui::MouseEvent& event) {
  if (button_->state() == Button::STATE_DISABLED)
    return true;
  if (button_->state() != Button::STATE_PRESSED &&
      button_controller_delegate_->ShouldEnterPushedState(event) &&
      button_->HitTestPoint(event.location())) {
    button_->SetState(Button::STATE_PRESSED);
    button_->AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
  }
  button_controller_delegate_->RequestFocusFromEvent();
  if (button_controller_delegate_->IsTriggerableEvent(event) &&
      notify_action_ == ButtonController::NotifyAction::kOnPress) {
    button_controller_delegate_->NotifyClick(event);
    // NOTE: We may be deleted at this point (by the listener's notification
    // handler).
  }
  return true;
}

void ButtonController::OnMouseReleased(const ui::MouseEvent& event) {
  if (button_->state() != Button::STATE_DISABLED) {
    if (!button_->HitTestPoint(event.location())) {
      button_->SetState(Button::STATE_NORMAL);
    } else {
      button_->SetState(Button::STATE_HOVERED);
      if (button_controller_delegate_->IsTriggerableEvent(event) &&
          notify_action_ == ButtonController::NotifyAction::kOnRelease) {
        button_controller_delegate_->NotifyClick(event);
        // NOTE: We may be deleted at this point (by the listener's notification
        // handler).
        return;
      }
    }
  }
  if (notify_action_ == ButtonController::NotifyAction::kOnRelease)
    button_controller_delegate_->OnClickCanceled(event);
}

void ButtonController::OnMouseMoved(const ui::MouseEvent& event) {
  if (button_->state() != Button::STATE_DISABLED) {
    button_->SetState(button_->HitTestPoint(event.location())
                          ? Button::STATE_HOVERED
                          : Button::STATE_NORMAL);
  }
}

void ButtonController::OnMouseEntered(const ui::MouseEvent& event) {
  if (button_->state() != Button::STATE_DISABLED)
    button_->SetState(Button::STATE_HOVERED);
}

void ButtonController::OnMouseExited(const ui::MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (button_->state() != Button::STATE_DISABLED &&
      !button_controller_delegate_->InDrag())
    button_->SetState(Button::STATE_NORMAL);
}

bool ButtonController::OnKeyPressed(const ui::KeyEvent& event) {
  if (button_->state() == Button::STATE_DISABLED)
    return false;

  switch (button_->GetKeyClickActionForEvent(event)) {
    case Button::KeyClickAction::kOnKeyRelease:
      button_->SetState(Button::STATE_PRESSED);
      if (button_controller_delegate_->GetInkDrop()->GetTargetInkDropState() !=
          InkDropState::ACTION_PENDING) {
        button_->AnimateInkDrop(InkDropState::ACTION_PENDING,
                                nullptr /* event */);
      }
      return true;
    case Button::KeyClickAction::kOnKeyPress:
      button_->SetState(Button::STATE_NORMAL);
      button_controller_delegate_->NotifyClick(event);
      return true;
    case Button::KeyClickAction::kNone:
      return false;
  }

  NOTREACHED();
  return false;
}

bool ButtonController::OnKeyReleased(const ui::KeyEvent& event) {
  const bool click_button = button_->state() == Button::STATE_PRESSED &&
                            button_->GetKeyClickActionForEvent(event) ==
                                Button::KeyClickAction::kOnKeyRelease;
  if (!click_button)
    return false;

  button_->SetState(Button::STATE_NORMAL);
  button_controller_delegate_->NotifyClick(event);
  return true;
}

void ButtonController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      button_controller_delegate_->IsTriggerableEvent(*event)) {
    // A GESTURE_END event is issued immediately after ET_GESTURE_TAP and will
    // set the state to STATE_NORMAL beginning the fade out animation.
    button_->SetState(Button::STATE_HOVERED);
    button_controller_delegate_->NotifyClick(*event);
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
             button_controller_delegate_->ShouldEnterPushedState(*event)) {
    button_->SetState(Button::STATE_PRESSED);
    button_controller_delegate_->RequestFocusFromEvent();
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_END) {
    button_->SetState(Button::STATE_NORMAL);
  }
}

void ButtonController::UpdateAccessibleNodeData(ui::AXNodeData* node_data) {}

void ButtonController::OnStateChanged(Button::ButtonState old_state) {}

bool ButtonController::IsTriggerableEvent(const ui::Event& event) {
  return event.type() == ui::ET_GESTURE_TAP_DOWN ||
         event.type() == ui::ET_GESTURE_TAP ||
         (event.IsMouseEvent() &&
          (button_->triggerable_event_flags() & event.flags()) != 0);
}

}  // namespace views
