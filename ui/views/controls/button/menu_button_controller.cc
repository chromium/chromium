// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::TimeTicks;

namespace views {

namespace {
ui::EventType NotifyActionToMouseEventType(
    ButtonController::NotifyAction notify_action) {
  switch (notify_action) {
    case ButtonController::NotifyAction::kOnPress:
      return ui::EventType::kMousePressed;
    case ButtonController::NotifyAction::kOnRelease:
      return ui::EventType::kMouseReleased;
  }
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MenuButtonController::PressedLock
//
////////////////////////////////////////////////////////////////////////////////

MenuButtonController::PressedLock::PressedLock(
    MenuButtonController* menu_button_controller)
    : PressedLock(menu_button_controller, false, nullptr) {}

MenuButtonController::PressedLock::PressedLock(
    MenuButtonController* menu_button_controller,
    bool is_sibling_menu_show,
    const ui::LocatedEvent* event)
    : menu_button_controller_(
          menu_button_controller->weak_factory_.GetWeakPtr()) {
  menu_button_controller_->IncrementPressedLocked(is_sibling_menu_show, event);
}

std::unique_ptr<MenuButtonController::PressedLock>
MenuButtonController::TakeLock() {
  return TakeLock(false, nullptr);
}

std::unique_ptr<MenuButtonController::PressedLock>
MenuButtonController::TakeLock(bool is_sibling_menu_show,
                               const ui::LocatedEvent* event) {
  return std::make_unique<MenuButtonController::PressedLock>(
      this, is_sibling_menu_show, event);
}

MenuButtonController::PressedLock::~PressedLock() {
  if (menu_button_controller_)
    menu_button_controller_->DecrementPressedLocked();
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButtonController
//
////////////////////////////////////////////////////////////////////////////////

MenuButtonController::MenuButtonController(
    Button* button,
    Button::PressedCallback callback,
    std::unique_ptr<ButtonControllerDelegate> delegate)
    : ButtonController(button, std::move(delegate)),
      callback_(std::move(callback)) {
  // Triggers on button press by default, unless drag-and-drop is enabled, see
  // MenuButtonController::IsTriggerableEventType.
  set_notify_action(ButtonController::NotifyAction::kOnPress);
  button->GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
  button->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
}

MenuButtonController::~MenuButtonController() = default;

bool MenuButtonController::OnMousePressed(const ui::MouseEvent& event) {
  // Sets true if the amount of time since the last |menu_closed_time_| is
  // large enough for the current event to be considered an intentionally
  // different event.
  is_intentional_menu_trigger_ =
      (TimeTicks::Now() - menu_closed_time_) >= kMinimumTimeBetweenButtonClicks;

  if (button()->GetRequestFocusOnPress())
    button()->RequestFocus();
  if (button()->GetState() != Button::STATE_DISABLED &&
      button()->HitTestPoint(event.location()) && IsTriggerableEvent(event)) {
    return Activate(&event);
  }

  // If this is an unintentional trigger do not display the inkdrop.
  if (!is_intentional_menu_trigger_)
    InkDrop::Get(button())->AnimateToState(InkDropState::HIDDEN, &event);
  return true;
}

void MenuButtonController::OnMouseReleased(const ui::MouseEvent& event) {
  if (button()->GetState() != Button::STATE_DISABLED &&
      delegate()->IsTriggerableEvent(event) &&
      button()->HitTestPoint(event.location()) && !delegate()->InDrag()) {
    Activate(&event);
  } else {
    if (button()->GetHideInkDropWhenShowingContextMenu())
      InkDrop::Get(button())->AnimateToState(InkDropState::HIDDEN, &event);
    ButtonController::OnMouseReleased(event);
  }
}

void MenuButtonController::OnMouseMoved(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    ButtonController::OnMouseMoved(event);
}

void MenuButtonController::OnMouseEntered(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    ButtonController::OnMouseEntered(event);
}

void MenuButtonController::OnMouseExited(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    ButtonController::OnMouseExited(event);
}

bool MenuButtonController::OnKeyPressed(const ui::KeyEvent& event) {
  // Alt-space on windows should show the window menu.
  if (event.key_code() == ui::VKEY_SPACE && event.IsAltDown())
    return false;

  // If Return doesn't normally click buttons, don't do it here either.
  if (event.key_code() == ui::VKEY_RETURN &&
      !PlatformStyle::kReturnClicksFocusedControl) {
    return false;
  }

  switch (event.key_code()) {
    case ui::VKEY_SPACE:
    case ui::VKEY_RETURN:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      // WARNING: we may have been deleted by the time Activate returns.
      Activate(&event);
      // This is to prevent the keyboard event from being dispatched twice.  If
      // the keyboard event is not handled, we pass it to the default handler
      // which dispatches the event back to us causing the menu to get displayed
      // again. Return true to prevent this.
      return true;
    }
    default:
      break;
  }
  return false;
}

bool MenuButtonController::OnKeyReleased(const ui::KeyEvent& event) {
  // A MenuButton always activates the menu on key press.
  return false;
}

void MenuButtonController::UpdateButtonAccessibleDefaultActionVerb() {
  if (button()->GetEnabled()) {
    button()->GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kOpen);
  } else {
    button()->GetViewAccessibility().RemoveDefaultActionVerb();
  }
}

bool MenuButtonController::IsTriggerableEvent(const ui::Event& event) {
  return ButtonController::IsTriggerableEvent(event) &&
         IsTriggerableEventType(event) && is_intentional_menu_trigger_;
}

void MenuButtonController::OnGestureEvent(ui::GestureEvent* event) {
  if (button()->GetState() != Button::STATE_DISABLED) {
    auto ref = weak_factory_.GetWeakPtr();
    if (delegate()->IsTriggerableEvent(*event) && !Activate(event)) {
      // When Activate() returns false, it means the click was handled by a
      // button listener and has handled the gesture event. So, there is no need
      // to further process the gesture event here. However, if the listener
      // didn't run menu code, we should make sure to reset our state.
      if (ref && button()->GetState() == Button::STATE_HOVERED)
        button()->SetState(Button::STATE_NORMAL);

      return;
    }
    if (event->type() == ui::EventType::kGestureTapDown) {
      event->SetHandled();
      if (pressed_lock_count_ == 0)
        button()->SetState(Button::STATE_HOVERED);
    } else if (button()->GetState() == Button::STATE_HOVERED &&
               (event->type() == ui::EventType::kGestureTapCancel ||
                event->type() == ui::EventType::kGestureEnd) &&
               pressed_lock_count_ == 0) {
      button()->SetState(Button::STATE_NORMAL);
    }
  }
  ButtonController::OnGestureEvent(event);
}

bool MenuButtonController::Activate(const ui::Event* event) {
  if (callback_) {
    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    static_cast<internal::RootView*>(button()->GetWidget()->GetRootView())
        ->SetMouseAndGestureHandler(nullptr);

    DCHECK(increment_pressed_lock_called_ == nullptr);
    // Observe if IncrementPressedLocked() was called so we can trigger the
    // correct ink drop animations.
    bool increment_pressed_lock_called = false;
    increment_pressed_lock_called_ = &increment_pressed_lock_called;

    // Since regular Button logic isn't used, we need to instead notify that the
    // menu button was activated here.
    const ui::ElementIdentifier id =
        button()->GetProperty(views::kElementIdentifierKey);
    if (id) {
      views::ElementTrackerViews::GetInstance()->NotifyViewActivated(id,
                                                                     button());
    }

    // Allow for the button callback to delete this.
    auto ref = weak_factory_.GetWeakPtr();

    // TODO(pbos): Make sure we always propagate an event. This requires changes
    // to ShowAppMenu which now provides none.
    ui::KeyEvent fake_event(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                            ui::EF_IS_SYNTHESIZED);
    if (!event)
      event = &fake_event;
    // We don't set our state here. It's handled in the MenuController code or
    // by the callback.
    callback_.Run(*event);

    if (!ref) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    increment_pressed_lock_called_ = nullptr;

    if (!increment_pressed_lock_called && pressed_lock_count_ == 0) {
      InkDrop::Get(button())->AnimateToState(
          InkDropState::ACTION_TRIGGERED, ui::LocatedEvent::FromIfValid(event));
    }

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }

  InkDrop::Get(button())->AnimateToState(InkDropState::HIDDEN,
                                         ui::LocatedEvent::FromIfValid(event));
  return true;
}

bool MenuButtonController::IsTriggerableEventType(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    const auto* mouse_event = event.AsMouseEvent();
    // Check that the event has the correct flags the button specified can
    // trigger button actions. For example, menus should only active on left
    // mouse button, to prevent a menu from being activated when a right-click
    // would also activate a context menu.
    if (!(mouse_event->button_flags() & button()->GetTriggerableEventFlags()))
      return false;

    // Activate on release if dragging, otherwise activate based on
    // notify_action.
    ui::EventType active_on =
        delegate()->GetDragOperations(mouse_event->location()) ==
                ui::DragDropTypes::DRAG_NONE
            ? NotifyActionToMouseEventType(notify_action())
            : ui::EventType::kMouseReleased;
    return event.type() == active_on;
  }
  return event.type() == ui::EventType::kGestureTap;
}

void MenuButtonController::NotifyClick() {
  ButtonController::NotifyClick();
  Activate(nullptr);
}

void MenuButtonController::IncrementPressedLocked(
    bool snap_ink_drop_to_activated,
    const ui::LocatedEvent* event) {
  ++pressed_lock_count_;
  if (increment_pressed_lock_called_)
    *increment_pressed_lock_called_ = true;
  if (!state_changed_subscription_) {
    state_changed_subscription_ =
        button()->AddStateChangedCallback(base::BindRepeating(
            &MenuButtonController::OnButtonStateChangedWhilePressedLocked,
            base::Unretained(this)));
  }
  should_disable_after_press_ = button()->GetState() == Button::STATE_DISABLED;
  if (button()->GetState() != Button::STATE_PRESSED) {
    if (snap_ink_drop_to_activated)
      delegate()->GetInkDrop()->SnapToActivated();
    else
      InkDrop::Get(button())->AnimateToState(InkDropState::ACTIVATED, event);
  }
  button()->SetState(Button::STATE_PRESSED);
  delegate()->GetInkDrop()->SetHovered(false);
}

void MenuButtonController::DecrementPressedLocked() {
  --pressed_lock_count_;
  DCHECK_GE(pressed_lock_count_, 0);

  // If this was the last lock, manually reset state to the desired state.
  if (pressed_lock_count_ == 0) {
    menu_closed_time_ = TimeTicks::Now();
    state_changed_subscription_ = {};
    LabelButton::ButtonState desired_state = Button::STATE_NORMAL;
    if (should_disable_after_press_) {
      desired_state = Button::STATE_DISABLED;
      should_disable_after_press_ = false;
    } else if (button()->GetWidget() &&
               !button()->GetWidget()->dragged_view() &&
               delegate()->ShouldEnterHoveredState()) {
      desired_state = Button::STATE_HOVERED;
      delegate()->GetInkDrop()->SetHovered(true);
    }
    button()->SetState(desired_state);
    // The widget may be null during shutdown. If so, it doesn't make sense to
    // try to add an ink drop effect.
    if (button()->GetWidget() && button()->GetState() != Button::STATE_PRESSED)
      InkDrop::Get(button())->AnimateToState(InkDropState::DEACTIVATED,
                                             nullptr /* event */);
  }
}

void MenuButtonController::OnButtonStateChangedWhilePressedLocked() {
  // The button's state was changed while it was supposed to be locked in a
  // pressed state. This shouldn't happen, but conceivably could if a caller
  // tries to switch from enabled to disabled or vice versa while the button is
  // pressed.
  if (button()->GetState() == Button::STATE_NORMAL)
    should_disable_after_press_ = false;
  else if (button()->GetState() == Button::STATE_DISABLED)
    should_disable_after_press_ = true;
}

}  // namespace views
