// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/controls/button/button_observer.h"
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

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsButtonProperty, false)

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
// ButtonControllerDelegate:
Button::DefaultButtonControllerDelegate::DefaultButtonControllerDelegate(
    Button* button)
    : ButtonControllerDelegate(button) {}

Button::DefaultButtonControllerDelegate::~DefaultButtonControllerDelegate() =
    default;

void Button::DefaultButtonControllerDelegate::RequestFocusFromEvent() {
  button()->RequestFocusFromEvent();
}

void Button::DefaultButtonControllerDelegate::NotifyClick(
    const ui::Event& event) {
  button()->NotifyClick(event);
}

void Button::DefaultButtonControllerDelegate::OnClickCanceled(
    const ui::Event& event) {
  button()->OnClickCanceled(event);
}

bool Button::DefaultButtonControllerDelegate::IsTriggerableEvent(
    const ui::Event& event) {
  return button()->IsTriggerableEvent(event);
}

bool Button::DefaultButtonControllerDelegate::ShouldEnterPushedState(
    const ui::Event& event) {
  return button()->ShouldEnterPushedState(event);
}

bool Button::DefaultButtonControllerDelegate::ShouldEnterHoveredState() {
  return button()->ShouldEnterHoveredState();
}

InkDrop* Button::DefaultButtonControllerDelegate::GetInkDrop() {
  return button()->GetInkDrop();
}

int Button::DefaultButtonControllerDelegate::GetDragOperations(
    const gfx::Point& press_pt) {
  return button()->GetDragOperations(press_pt);
}

bool Button::DefaultButtonControllerDelegate::InDrag() {
  return button()->InDrag();
}

////////////////////////////////////////////////////////////////////////////////

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

Button::~Button() = default;

void Button::SetFocusForPlatform() {
#if defined(OS_MACOSX)
  // On Mac, buttons are focusable only in full keyboard access mode.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
}

void Button::SetTooltipText(const base::string16& tooltip_text) {
  if (tooltip_text == tooltip_text_)
    return;
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

void Button::SetAnimationDuration(base::TimeDelta duration) {
  hover_animation_.SetSlideDuration(duration);
}

void Button::SetInstallFocusRingOnFocus(bool install) {
  if (install)
    focus_ring_ = FocusRing::Install(this);
  else
    focus_ring_.reset();
}

void Button::SetHotTracked(bool is_hot_tracked) {
  if (state_ != STATE_DISABLED) {
    SetState(is_hot_tracked ? STATE_HOVERED : STATE_NORMAL);
    if (show_ink_drop_when_hot_tracked_) {
      AnimateInkDrop(is_hot_tracked ? views::InkDropState::ACTIVATED
                                    : views::InkDropState::HIDDEN,
                     nullptr);
    }
  }

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
  for (ButtonObserver& observer : button_observers_)
    observer.OnHighlightChanged(this, bubble_visible);
}

void Button::AddButtonObserver(ButtonObserver* observer) {
  button_observers_.AddObserver(observer);
}

void Button::RemoveButtonObserver(ButtonObserver* observer) {
  button_observers_.RemoveObserver(observer);
}

Button::KeyClickAction Button::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return PlatformStyle::kKeyClickActionOnSpace;
  if (event.key_code() == ui::VKEY_RETURN &&
      PlatformStyle::kReturnClicksFocusedControl)
    return KeyClickAction::kOnKeyPress;
  return KeyClickAction::kNone;
}

void Button::SetButtonController(
    std::unique_ptr<ButtonController> button_controller) {
  button_controller_ = std::move(button_controller);
}

gfx::Point Button::GetMenuPosition() const {
  gfx::Rect lb = GetLocalBounds();

  // Offset of the associated menu position.
  constexpr gfx::Vector2d kMenuOffset{-2, -4};

  // The position of the menu depends on whether or not the locale is
  // right-to-left.
  gfx::Point menu_position(lb.right(), lb.bottom());
  if (base::i18n::IsRTL())
    menu_position.set_x(lb.x());

  View::ConvertPointToScreen(this, &menu_position);
  if (base::i18n::IsRTL())
    menu_position.Offset(-kMenuOffset.x(), kMenuOffset.y());
  else
    menu_position += kMenuOffset;

  DCHECK(GetWidget());
  const int max_x_coordinate =
      GetWidget()->GetWorkAreaBoundsInScreen().right() - 1;
  if (max_x_coordinate && max_x_coordinate <= menu_position.x())
    menu_position.set_x(max_x_coordinate - 1);
  return menu_position;
}

////////////////////////////////////////////////////////////////////////////////
// Button, View overrides:

bool Button::OnMousePressed(const ui::MouseEvent& event) {
  return button_controller_->OnMousePressed(event);
}

bool Button::OnMouseDragged(const ui::MouseEvent& event) {
  if (state_ != STATE_DISABLED) {
    const bool should_enter_pushed = ShouldEnterPushedState(event);
    const bool should_show_pending =
        should_enter_pushed &&
        button_controller_->notify_action() ==
            ButtonController::NotifyAction::kOnRelease &&
        !InDrag();
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
  button_controller_->OnMouseReleased(event);
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
  button_controller_->OnMouseEntered(event);
}

void Button::OnMouseExited(const ui::MouseEvent& event) {
  button_controller_->OnMouseExited(event);
}

void Button::OnMouseMoved(const ui::MouseEvent& event) {
  button_controller_->OnMouseMoved(event);
}

bool Button::OnKeyPressed(const ui::KeyEvent& event) {
  return button_controller_->OnKeyPressed(event);
}

bool Button::OnKeyReleased(const ui::KeyEvent& event) {
  return button_controller_->OnKeyReleased(event);
}

void Button::OnGestureEvent(ui::GestureEvent* event) {
  button_controller_->OnGestureEvent(event);
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
  return GetKeyClickActionForEvent(event) != KeyClickAction::kNone;
}

base::string16 Button::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
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
  if (!GetEnabled())
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
  if (GetEnabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);

  button_controller_->UpdateAccessibleNodeData(node_data);
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
  std::unique_ptr<InkDrop> ink_drop = InkDropHostView::CreateInkDrop();
  ink_drop->SetShowHighlightOnFocus(!focus_ring_);
  return ink_drop;
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
    : AnimationDelegateViews(this),
      listener_(listener),
      ink_drop_base_color_(gfx::kPlaceholderColor) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetProperty(kIsButtonProperty, true);
  hover_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(150));
  SetInstallFocusRingOnFocus(PlatformStyle::kPreferFocusRings);
  button_controller_ = std::make_unique<ButtonController>(
      this, std::make_unique<DefaultButtonControllerDelegate>(this));
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

void Button::StateChanged(ButtonState old_state) {
  button_controller_->OnStateChanged(old_state);
  for (ButtonObserver& observer : button_observers_)
    observer.OnStateChanged(this, old_state);
}

bool Button::IsTriggerableEvent(const ui::Event& event) {
  return button_controller_->IsTriggerableEvent(event);
}

bool Button::ShouldUpdateInkDropOnClickCanceled() const {
  return true;
}

bool Button::ShouldEnterPushedState(const ui::Event& event) {
  return IsTriggerableEvent(event);
}

void Button::PaintButtonContents(gfx::Canvas* canvas) {}

bool Button::ShouldEnterHoveredState() {
  if (!GetVisible())
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

void Button::OnEnabledChanged() {
  if (GetEnabled() ? (state_ != STATE_DISABLED) : (state_ == STATE_DISABLED))
    return;

  if (GetEnabled()) {
    bool should_enter_hover_state = ShouldEnterHoveredState();
    SetState(should_enter_hover_state ? STATE_HOVERED : STATE_NORMAL);
    GetInkDrop()->SetHovered(should_enter_hover_state);
  } else {
    SetState(STATE_DISABLED);
    GetInkDrop()->SetHovered(false);
  }
}

void Button::WidgetActivationChanged(Widget* widget, bool active) {
  StateChanged(state());
}

BEGIN_METADATA(Button)
METADATA_PARENT_CLASS(InkDropHostView)
END_METADATA()

}  // namespace views
