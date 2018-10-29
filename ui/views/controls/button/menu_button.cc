// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::TimeTicks;
using base::TimeDelta;

namespace views {

// Default menu offset.
static const int kDefaultMenuOffsetX = -2;
static const int kDefaultMenuOffsetY = -4;

// static
const char MenuButton::kViewClassName[] = "MenuButton";
const int MenuButton::kMenuMarkerPaddingLeft = 3;
const int MenuButton::kMenuMarkerPaddingRight = -1;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton::PressedLock
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::PressedLock::PressedLock(MenuButton* menu_button)
    : PressedLock(menu_button, false, nullptr) {}

MenuButton::PressedLock::PressedLock(MenuButton* menu_button,
                                     bool is_sibling_menu_show,
                                     const ui::LocatedEvent* event)
    : menu_button_(menu_button->weak_factory_.GetWeakPtr()) {
  menu_button_->IncrementPressedLocked(is_sibling_menu_show, event);
}

MenuButton::PressedLock::~PressedLock() {
  if (menu_button_)
    menu_button_->DecrementPressedLocked();
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(const base::string16& text,
                       MenuButtonListener* menu_button_listener,
                       bool show_menu_marker)
    : LabelButton(nullptr, text),
      menu_offset_(kDefaultMenuOffsetX, kDefaultMenuOffsetY),
      listener_(menu_button_listener),
      show_menu_marker_(show_menu_marker),
      menu_marker_(ui::ResourceBundle::GetSharedInstance()
                       .GetImageNamed(IDR_MENU_DROPARROW)
                       .ToImageSkia()),
      weak_factory_(this) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

MenuButton::~MenuButton() = default;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

bool MenuButton::Activate(const ui::Event* event) {
  if (listener_) {
    gfx::Rect lb = GetLocalBounds();

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (base::i18n::IsRTL())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(this, &menu_position);
    if (base::i18n::IsRTL())
      menu_position.Offset(-menu_offset_.x(), menu_offset_.y());
    else
      menu_position.Offset(menu_offset_.x(), menu_offset_.y());

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    static_cast<internal::RootView*>(GetWidget()->GetRootView())
        ->SetMouseHandler(nullptr);

    DCHECK(increment_pressed_lock_called_ == nullptr);
    // Observe if IncrementPressedLocked() was called so we can trigger the
    // correct ink drop animations.
    bool increment_pressed_lock_called = false;
    increment_pressed_lock_called_ = &increment_pressed_lock_called;

    // Allow for OnMenuButtonClicked() to delete this.
    auto ref = weak_factory_.GetWeakPtr();

    // We don't set our state here. It's handled in the MenuController code or
    // by our click listener.
    listener_->OnMenuButtonClicked(this, menu_position, event);

    if (!ref) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    increment_pressed_lock_called_ = nullptr;

    if (!increment_pressed_lock_called && pressed_lock_count_ == 0) {
      AnimateInkDrop(InkDropState::ACTION_TRIGGERED,
                     ui::LocatedEvent::FromIfValid(event));
    }

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }

  AnimateInkDrop(InkDropState::HIDDEN, ui::LocatedEvent::FromIfValid(event));
  return true;
}

bool MenuButton::IsTriggerableEventType(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& mouseev = static_cast<const ui::MouseEvent&>(event);
    // Active on left mouse button only, to prevent a menu from being activated
    // when a right-click would also activate a context menu.
    if (!mouseev.IsOnlyLeftMouseButton())
      return false;
    // If dragging is supported activate on release, otherwise activate on
    // pressed.
    ui::EventType active_on =
        GetDragOperations(mouseev.location()) == ui::DragDropTypes::DRAG_NONE
            ? ui::ET_MOUSE_PRESSED
            : ui::ET_MOUSE_RELEASED;
    return event.type() == active_on;
  }

  return event.type() == ui::ET_GESTURE_TAP;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

gfx::Size MenuButton::CalculatePreferredSize() const {
  gfx::Size prefsize = LabelButton::CalculatePreferredSize();
  if (show_menu_marker_) {
    prefsize.Enlarge(menu_marker_->width() + kMenuMarkerPaddingLeft +
                         kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

const char* MenuButton::GetClassName() const {
  return kViewClassName;
}

bool MenuButton::OnMousePressed(const ui::MouseEvent& event) {
  if (request_focus_on_press())
    RequestFocus();
  if (state() != STATE_DISABLED && HitTestPoint(event.location()) &&
      IsTriggerableEventType(event)) {
    if (IsTriggerableEvent(event))
      return Activate(&event);
  }
  return true;
}

void MenuButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (state() != STATE_DISABLED && IsTriggerableEvent(event) &&
      HitTestPoint(event.location()) && !InDrag()) {
    Activate(&event);
  } else {
    AnimateInkDrop(InkDropState::HIDDEN, &event);
    LabelButton::OnMouseReleased(event);
  }
}

void MenuButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseEntered(event);
}

void MenuButton::OnMouseExited(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseExited(event);
}

void MenuButton::OnMouseMoved(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    LabelButton::OnMouseMoved(event);
}

void MenuButton::OnGestureEvent(ui::GestureEvent* event) {
  if (state() != STATE_DISABLED) {
    auto ref = weak_factory_.GetWeakPtr();
    if (IsTriggerableEvent(*event) && !Activate(event)) {
      if (!ref)
        return;
      // When |Activate()| returns |false|, it means the click was handled by
      // a button listener and has handled the gesture event. So, there is no
      // need to further process the gesture event here. However, if the
      // listener didn't run menu code, we should make sure to reset our state.
      if (state() == Button::STATE_HOVERED)
        SetState(Button::STATE_NORMAL);
      return;
    }
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      event->SetHandled();
      if (pressed_lock_count_ == 0)
        SetState(Button::STATE_HOVERED);
    } else if (state() == Button::STATE_HOVERED &&
               (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
                event->type() == ui::ET_GESTURE_END) &&
               pressed_lock_count_ == 0) {
      SetState(Button::STATE_NORMAL);
    }
  }
  LabelButton::OnGestureEvent(event);
}

bool MenuButton::OnKeyPressed(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_SPACE:
      // Alt-space on windows should show the window menu.
      if (event.IsAltDown())
        break;
      FALLTHROUGH;
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

bool MenuButton::OnKeyReleased(const ui::KeyEvent& event) {
  // Override Button's implementation, which presses the button when
  // you press space and clicks it when you release space.  For a MenuButton
  // we always activate the menu on key press.
  return false;
}

void MenuButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kPopUpButton;
  node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
  if (enabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
}

void MenuButton::PaintMenuMarker(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  // Using the Views mirroring infrastructure incorrectly flips icon content.
  // Instead, manually mirror the position of the down arrow.
  gfx::Rect arrow_bounds(width() - insets.right() -
                         menu_marker_->width() - kMenuMarkerPaddingRight,
                         height() / 2 - menu_marker_->height() / 2,
                         menu_marker_->width(),
                         menu_marker_->height());
  arrow_bounds.set_x(GetMirroredXForRect(arrow_bounds));
  canvas->DrawImageInt(*menu_marker_, arrow_bounds.x(), arrow_bounds.y());
}

gfx::Rect MenuButton::GetChildAreaBounds() {
  gfx::Size s = size();

  if (show_menu_marker_) {
    s.set_width(s.width() - menu_marker_->width() - kMenuMarkerPaddingLeft -
                kMenuMarkerPaddingRight);
  }

  return gfx::Rect(s);
}

bool MenuButton::IsTriggerableEvent(const ui::Event& event) {
  if (!IsTriggerableEventType(event))
    return false;

  TimeDelta delta = TimeTicks::Now() - menu_closed_time_;
  if (delta.InMilliseconds() < kMinimumMsBetweenButtonClicks)
    return false;  // Not enough time since the menu closed.

  return true;
}

bool MenuButton::ShouldEnterPushedState(const ui::Event& event) {
  return IsTriggerableEventType(event);
}

void MenuButton::StateChanged(ButtonState old_state) {
  if (pressed_lock_count_ != 0) {
    // The button's state was changed while it was supposed to be locked in a
    // pressed state. This shouldn't happen, but conceivably could if a caller
    // tries to switch from enabled to disabled or vice versa while the button
    // is pressed.
    if (state() == STATE_NORMAL)
      should_disable_after_press_ = false;
    else if (state() == STATE_DISABLED)
      should_disable_after_press_ = true;
  } else {
    LabelButton::StateChanged(old_state);
  }
}

void MenuButton::NotifyClick(const ui::Event& event) {
  // We don't forward events to the normal button listener, instead using the
  // MenuButtonListener.
  Activate(&event);
}

void MenuButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (show_menu_marker_)
    PaintMenuMarker(canvas);
}

void MenuButton::IncrementPressedLocked(bool snap_ink_drop_to_activated,
                                        const ui::LocatedEvent* event) {
  ++pressed_lock_count_;
  if (increment_pressed_lock_called_)
    *increment_pressed_lock_called_ = true;
  should_disable_after_press_ = state() == STATE_DISABLED;
  if (state() != STATE_PRESSED) {
    if (snap_ink_drop_to_activated)
      GetInkDrop()->SnapToActivated();
    else
      AnimateInkDrop(InkDropState::ACTIVATED, event);
  }
  SetState(STATE_PRESSED);
  GetInkDrop()->SetHovered(false);
}

void MenuButton::DecrementPressedLocked() {
  --pressed_lock_count_;
  DCHECK_GE(pressed_lock_count_, 0);

  // If this was the last lock, manually reset state to the desired state.
  if (pressed_lock_count_ == 0) {
    menu_closed_time_ = TimeTicks::Now();
    ButtonState desired_state = STATE_NORMAL;
    if (should_disable_after_press_) {
      desired_state = STATE_DISABLED;
      should_disable_after_press_ = false;
    } else if (GetWidget() && !GetWidget()->dragged_view() &&
               ShouldEnterHoveredState()) {
      desired_state = STATE_HOVERED;
      GetInkDrop()->SetHovered(true);
    }
    SetState(desired_state);
    // The widget may be null during shutdown. If so, it doesn't make sense to
    // try to add an ink drop effect.
    if (GetWidget() && state() != STATE_PRESSED)
      AnimateInkDrop(InkDropState::DEACTIVATED, nullptr /* event */);
  }
}

int MenuButton::GetMaximumScreenXCoordinate() {
  if (!GetWidget()) {
    NOTREACHED();
    return 0;
  }

  gfx::Rect monitor_bounds = GetWidget()->GetWorkAreaBoundsInScreen();
  return monitor_bounds.right() - 1;
}

}  // namespace views
