// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/class_property.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#endif

namespace views {

namespace {

DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(bool, kIsButtonProperty, false);

// How long the hover animation takes if uninterrupted.
const int kHoverFadeDurationMs = 150;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WidgetObserverButtonBridge:
Button::WidgetObserverButtonBridge::WidgetObserverButtonBridge(Button* button)
    : owner_(button) {
  DCHECK(button->GetWidget());
  button->GetWidget()->AddObserver(this);
}

Button::WidgetObserverButtonBridge::~WidgetObserverButtonBridge() {
  if (owner_)
    owner_->GetWidget()->RemoveObserver(this);
}

void Button::WidgetObserverButtonBridge::OnWidgetActivationChanged(
    Widget* widget,
    bool active) {
  owner_->WidgetActivationChanged(widget, active);
}

void Button::WidgetObserverButtonBridge::OnWidgetDestroying(Widget* widget) {
  widget->RemoveObserver(this);
  owner_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Button, static public:

// static
const char Button::kViewClassName[] = "Button";

// static
const Button* Button::AsButton(const views::View* view) {
  return AsButton(const_cast<View*>(view));
}

// static
Button* Button::AsButton(views::View* view) {
  if (view && view->GetProperty(kIsButtonProperty))
    return static_cast<Button*>(view);
  return nullptr;
}

// static
Button::ButtonState Button::GetButtonStateFrom(ui::NativeTheme::State state) {
  switch (state) {
    case ui::NativeTheme::kDisabled:  return Button::STATE_DISABLED;
    case ui::NativeTheme::kHovered:   return Button::STATE_HOVERED;
    case ui::NativeTheme::kNormal:    return Button::STATE_NORMAL;
    case ui::NativeTheme::kPressed:   return Button::STATE_PRESSED;
    case ui::NativeTheme::kNumStates: NOTREACHED();
  }
  return Button::STATE_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////
// Button, public:

Button::~Button() {}

void Button::SetFocusForPlatform() {
#if defined(OS_MACOSX)
  // On Mac, buttons are focusable only in full keyboard access mode.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
}

void Button::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
  OnSetTooltipText(tooltip_text);
  TooltipTextChanged();
}

void Button::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
  NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

const base::string16& Button::GetAccessibleName() const {
  return accessible_name_.empty() ? tooltip_text_ : accessible_name_;
}

void Button::SetState(ButtonState state) {
  if (state == state_)
    return;

  if (animate_on_state_change_ &&
      (!is_throbbing_ || !hover_animation_.is_animating())) {
    is_throbbing_ = false;
    if ((state_ == STATE_HOVERED) && (state == STATE_NORMAL)) {
      // For HOVERED -> NORMAL, animate from hovered (1) to not hovered (0).
      hover_animation_.Hide();
    } else if (state != STATE_HOVERED) {
      // For HOVERED -> PRESSED/DISABLED, or any transition not involving
      // HOVERED at all, simply set the state to not hovered (0).
      hover_animation_.Reset();
    } else if (state_ == STATE_NORMAL) {
      // For NORMAL -> HOVERED, animate from not hovered (0) to hovered (1).
      hover_animation_.Show();
    } else {
      // For PRESSED/DISABLED -> HOVERED, simply set the state to hovered (1).
      hover_animation_.Reset(1);
    }
  }

  ButtonState old_state = state_;
  state_ = state;
  StateChanged(old_state);
  SchedulePaint();
}

Button::ButtonState Button::GetVisualState() const {
  if (PlatformStyle::kInactiveWidgetControlsAppearDisabled && GetWidget() &&
      !GetWidget()->IsActive()) {
    return STATE_DISABLED;
  }
  return state();
}

void Button::StartThrobbing(int cycles_til_stop) {
  if (!animate_on_state_change_)
    return;
  is_throbbing_ = true;
  hover_animation_.StartThrobbing(cycles_til_stop);
}

void Button::StopThrobbing() {
  if (hover_animation_.is_animating()) {
    hover_animation_.Stop();
    SchedulePaint();
  }
}

void Button::SetAnimationDuration(int duration) {
  hover_animation_.SetSlideDuration(duration);
}

void Button::SetInstallFocusRingOnFocus(bool install) {
  if (install)
    focus_ring_ = FocusRing::Install(this);
  else
    focus_ring_.reset();
}

void Button::SetHotTracked(bool is_hot_tracked) {
  if (state_ != STATE_DISABLED)
    SetState(is_hot_tracked ? STATE_HOVERED : STATE_NORMAL);

  if (is_hot_tracked)
    NotifyAccessibilityEvent(ax::mojom::Event::kHover, true);
}

bool Button::IsHotTracked() const {
  return state_ == STATE_HOVERED;
}

void Button::SetFocusPainter(std::unique_ptr<Painter> focus_painter) {
  focus_painter_ = std::move(focus_painter);
}

void Button::SetHighlighted(bool bubble_visible) {
  AnimateInkDrop(bubble_visible ? views::InkDropState::ACTIVATED
                                : views::InkDropState::DEACTIVATED,
                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Button, View overrides:

void Button::OnEnabledChanged() {
  if (enabled() ? (state_ != STATE_DISABLED) : (state_ == STATE_DISABLED))
    return;

  if (enabled()) {
    bool should_enter_hover_state = ShouldEnterHoveredState();
    SetState(should_enter_hover_state ? STATE_HOVERED : STATE_NORMAL);
    GetInkDrop()->SetHovered(should_enter_hover_state);
  } else {
    SetState(STATE_DISABLED);
    GetInkDrop()->SetHovered(false);
  }
}

const char* Button::GetClassName() const {
  return kViewClassName;
}

bool Button::OnMousePressed(const ui::MouseEvent& event) {
  if (state_ == STATE_DISABLED)
    return true;
  if (state_ != STATE_PRESSED && ShouldEnterPushedState(event) &&
      HitTestPoint(event.location())) {
    SetState(STATE_PRESSED);
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
  }
  RequestFocusFromEvent();
  if (IsTriggerableEvent(event) && notify_action_ == NOTIFY_ON_PRESS) {
    NotifyClick(event);
    // NOTE: We may be deleted at this point (by the listener's notification
    // handler).
  }
  return true;
}

bool Button::OnMouseDragged(const ui::MouseEvent& event) {
  if (state_ != STATE_DISABLED) {
    const bool should_enter_pushed = ShouldEnterPushedState(event);
    const bool should_show_pending =
        should_enter_pushed && notify_action_ == NOTIFY_ON_RELEASE && !InDrag();
    if (HitTestPoint(event.location())) {
      SetState(should_enter_pushed ? STATE_PRESSED : STATE_HOVERED);
      if (should_show_pending && GetInkDrop()->GetTargetInkDropState() ==
                                     views::InkDropState::HIDDEN) {
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
      }
    } else {
      SetState(STATE_NORMAL);
      if (should_show_pending && GetInkDrop()->GetTargetInkDropState() ==
                                     views::InkDropState::ACTION_PENDING) {
        AnimateInkDrop(views::InkDropState::HIDDEN, &event);
      }
    }
  }
  return true;
}

void Button::OnMouseReleased(const ui::MouseEvent& event) {
  if (state_ != STATE_DISABLED) {
    if (!HitTestPoint(event.location())) {
      SetState(STATE_NORMAL);
    } else {
      SetState(STATE_HOVERED);
      if (IsTriggerableEvent(event) && notify_action_ == NOTIFY_ON_RELEASE) {
        NotifyClick(event);
        // NOTE: We may be deleted at this point (by the listener's notification
        // handler).
        return;
      }
    }
  }
  if (notify_action_ == NOTIFY_ON_RELEASE)
    OnClickCanceled(event);
}

void Button::OnMouseCaptureLost() {
  // Starting a drag results in a MouseCaptureLost. Reset button state.
  // TODO(varkha): Reset the state even while in drag. The same logic may
  // applies everywhere so gather any feedback and update.
  if (state_ != STATE_DISABLED)
    SetState(STATE_NORMAL);
  AnimateInkDrop(views::InkDropState::HIDDEN, nullptr /* event */);
  GetInkDrop()->SetHovered(false);
  InkDropHostView::OnMouseCaptureLost();
}

void Button::OnMouseEntered(const ui::MouseEvent& event) {
  if (state_ != STATE_DISABLED)
    SetState(STATE_HOVERED);
}

void Button::OnMouseExited(const ui::MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (state_ != STATE_DISABLED && !InDrag())
    SetState(STATE_NORMAL);
}

void Button::OnMouseMoved(const ui::MouseEvent& event) {
  if (state_ != STATE_DISABLED)
    SetState(HitTestPoint(event.location()) ? STATE_HOVERED : STATE_NORMAL);
}

bool Button::OnKeyPressed(const ui::KeyEvent& event) {
  if (state_ == STATE_DISABLED)
    return false;

  switch (GetKeyClickActionForEvent(event)) {
    case KeyClickAction::CLICK_ON_KEY_RELEASE:
      SetState(STATE_PRESSED);
      if (GetInkDrop()->GetTargetInkDropState() !=
          InkDropState::ACTION_PENDING) {
        AnimateInkDrop(InkDropState::ACTION_PENDING, nullptr /* event */);
      }
      return true;
    case KeyClickAction::CLICK_ON_KEY_PRESS:
      SetState(STATE_NORMAL);
      NotifyClick(event);
      return true;
    case KeyClickAction::CLICK_NONE:
      return false;
  }

  NOTREACHED();
  return false;
}

bool Button::OnKeyReleased(const ui::KeyEvent& event) {
  const bool click_button =
      state_ == STATE_PRESSED &&
      GetKeyClickActionForEvent(event) == KeyClickAction::CLICK_ON_KEY_RELEASE;
  if (!click_button)
    return false;

  SetState(STATE_NORMAL);
  NotifyClick(event);
  return true;
}

void Button::OnGestureEvent(ui::GestureEvent* event) {
  if (state_ == STATE_DISABLED) {
    InkDropHostView::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP && IsTriggerableEvent(*event)) {
    // Set the button state to hot and start the animation fully faded in. The
    // GESTURE_END event issued immediately after will set the state to
    // STATE_NORMAL beginning the fade out animation. See
    // http://crbug.com/131184.
    SetState(STATE_HOVERED);
    hover_animation_.Reset(1.0);
    NotifyClick(*event);
    event->StopPropagation();
  } else if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
             ShouldEnterPushedState(*event)) {
    SetState(STATE_PRESSED);
    RequestFocusFromEvent();
    event->StopPropagation();
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_END) {
    SetState(STATE_NORMAL);
  }
  if (!event->handled())
    InkDropHostView::OnGestureEvent(event);
}

bool Button::AcceleratorPressed(const ui::Accelerator& accelerator) {
  SetState(STATE_NORMAL);
  NotifyClick(accelerator.ToKeyEvent());
  return true;
}

bool Button::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // If this button is focused and the user presses space or enter, don't let
  // that be treated as an accelerator if there is a key click action
  // corresponding to it.
  return GetKeyClickActionForEvent(event) != KeyClickAction::CLICK_NONE;
}

bool Button::GetTooltipText(const gfx::Point& p,
                            base::string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

void Button::ShowContextMenu(const gfx::Point& p,
                             ui::MenuSourceType source_type) {
  if (!context_menu_controller())
    return;

  // We're about to show the context menu. Showing the context menu likely means
  // we won't get a mouse exited and reset state. Reset it now to be sure.
  if (state_ != STATE_DISABLED)
    SetState(STATE_NORMAL);
  if (hide_ink_drop_when_showing_context_menu_) {
    GetInkDrop()->SetHovered(false);
    AnimateInkDrop(InkDropState::HIDDEN, nullptr /* event */);
  }
  InkDropHostView::ShowContextMenu(p, source_type);
}

void Button::OnDragDone() {
  // Only reset the state to normal if the button isn't currently disabled
  // (since disabled buttons may still be able to be dragged).
  if (state_ != STATE_DISABLED)
    SetState(STATE_NORMAL);
  AnimateInkDrop(InkDropState::HIDDEN, nullptr /* event */);
}

void Button::OnPaint(gfx::Canvas* canvas) {
  InkDropHostView::OnPaint(canvas);
  PaintButtonContents(canvas);
  Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void Button::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(GetAccessibleName());
  if (!enabled())
    node_data->SetRestriction(ax::mojom::Restriction::kDisabled);

  switch (state_) {
    case STATE_HOVERED:
      node_data->AddState(ax::mojom::State::kHovered);
      break;
    case STATE_PRESSED:
      node_data->SetCheckedState(ax::mojom::CheckedState::kTrue);
      break;
    case STATE_DISABLED:
      node_data->SetRestriction(ax::mojom::Restriction::kDisabled);
      break;
    case STATE_NORMAL:
    case STATE_COUNT:
      // No additional accessibility node_data set for this button node_data.
      break;
  }
  if (enabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);
}

void Button::VisibilityChanged(View* starting_from, bool visible) {
  InkDropHostView::VisibilityChanged(starting_from, visible);
  if (state_ == STATE_DISABLED)
    return;
  SetState(visible && ShouldEnterHoveredState() ? STATE_HOVERED : STATE_NORMAL);
}

void Button::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  if (!details.is_add && state_ != STATE_DISABLED && details.child == this)
    SetState(STATE_NORMAL);
  InkDropHostView::ViewHierarchyChanged(details);
}

void Button::OnFocus() {
  InkDropHostView::OnFocus();
  if (focus_painter_)
    SchedulePaint();
}

void Button::OnBlur() {
  InkDropHostView::OnBlur();
  if (IsHotTracked() || state_ == STATE_PRESSED) {
    SetState(STATE_NORMAL);
    if (GetInkDrop()->GetTargetInkDropState() != views::InkDropState::HIDDEN)
      AnimateInkDrop(views::InkDropState::HIDDEN, nullptr /* event */);
    // TODO(bruthig) : Fix Buttons to work well when multiple input
    // methods are interacting with a button. e.g. By animating to HIDDEN here
    // it is possible for a Mouse Release to trigger an action however there
    // would be no visual cue to the user that this will occur.
  }
  if (focus_painter_)
    SchedulePaint();
}

void Button::AddedToWidget() {
  if (PlatformStyle::kInactiveWidgetControlsAppearDisabled)
    widget_observer_ = std::make_unique<WidgetObserverButtonBridge>(this);
}

void Button::RemovedFromWidget() {
  widget_observer_.reset();
}

std::unique_ptr<InkDrop> Button::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop = CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(!focus_ring_);
  ink_drop->SetAutoHighlightModeForPlatform();
  return std::move(ink_drop);
}

SkColor Button::GetInkDropBaseColor() const {
  return ink_drop_base_color_;
}

////////////////////////////////////////////////////////////////////////////////
// Button, gfx::AnimationDelegate implementation:

void Button::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// Button, protected:

Button::Button(ButtonListener* listener)
    : listener_(listener), ink_drop_base_color_(gfx::kPlaceholderColor) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetProperty(kIsButtonProperty, true);
  hover_animation_.SetSlideDuration(kHoverFadeDurationMs);
  SetInstallFocusRingOnFocus(PlatformStyle::kPreferFocusRings);
}

Button::KeyClickAction Button::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return PlatformStyle::kKeyClickActionOnSpace;
  if (event.key_code() == ui::VKEY_RETURN &&
      PlatformStyle::kReturnClicksFocusedControl)
    return CLICK_ON_KEY_PRESS;
  return CLICK_NONE;
}

void Button::RequestFocusFromEvent() {
  if (request_focus_on_press_)
    RequestFocus();
}

void Button::NotifyClick(const ui::Event& event) {
  if (has_ink_drop_action_on_click_) {
    AnimateInkDrop(InkDropState::ACTION_TRIGGERED,
                   ui::LocatedEvent::FromIfValid(&event));
  }
  // We can be called when there is no listener, in cases like double clicks on
  // menu buttons etc.
  if (listener_)
    listener_->ButtonPressed(this, event);
}

void Button::OnClickCanceled(const ui::Event& event) {
  if (ShouldUpdateInkDropOnClickCanceled()) {
    if (GetInkDrop()->GetTargetInkDropState() ==
            views::InkDropState::ACTION_PENDING ||
        GetInkDrop()->GetTargetInkDropState() ==
            views::InkDropState::ALTERNATE_ACTION_PENDING) {
      AnimateInkDrop(views::InkDropState::HIDDEN,
                     ui::LocatedEvent::FromIfValid(&event));
    }
  }
}

void Button::OnSetTooltipText(const base::string16& tooltip_text) {}

void Button::StateChanged(ButtonState old_state) {}

bool Button::IsTriggerableEvent(const ui::Event& event) {
  return event.type() == ui::ET_GESTURE_TAP_DOWN ||
         event.type() == ui::ET_GESTURE_TAP ||
         (event.IsMouseEvent() &&
          (triggerable_event_flags_ & event.flags()) != 0);
}

bool Button::ShouldUpdateInkDropOnClickCanceled() const {
  return true;
}

bool Button::ShouldEnterPushedState(const ui::Event& event) {
  return IsTriggerableEvent(event);
}

void Button::PaintButtonContents(gfx::Canvas* canvas) {}

bool Button::ShouldEnterHoveredState() {
  if (!visible())
    return false;

  bool check_mouse_position = true;
#if defined(USE_AURA)
  // If another window has capture, we shouldn't check the current mouse
  // position because the button won't receive any mouse events - so if the
  // mouse was hovered, the button would be stuck in a hovered state (since it
  // would never receive OnMouseExited).
  const Widget* widget = GetWidget();
  if (widget && widget->GetNativeWindow()) {
    aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
    aura::client::CaptureClient* capture_client =
        aura::client::GetCaptureClient(root_window);
    aura::Window* capture_window =
        capture_client ? capture_client->GetGlobalCaptureWindow() : nullptr;
    check_mouse_position = !capture_window || capture_window == root_window;
  }
#endif

  return check_mouse_position && IsMouseHovered();
}

void Button::WidgetActivationChanged(Widget* widget, bool active) {
  StateChanged(state());
}

}  // namespace views
