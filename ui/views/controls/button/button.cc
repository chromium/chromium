// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

#if defined(USE_AURA)
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#endif

namespace views {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsButtonProperty, false)

}  // namespace

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
  return InkDrop::Get(button()->ink_drop_view())->GetInkDrop();
}

int Button::DefaultButtonControllerDelegate::GetDragOperations(
    const gfx::Point& press_pt) {
  return button()->GetDragOperations(press_pt);
}

bool Button::DefaultButtonControllerDelegate::InDrag() {
  return button()->InDrag();
}

Button::PressedCallback::PressedCallback(base::OnceClosure closure)
    : callback_(std::move(closure)) {}

Button::PressedCallback::PressedCallback(
    Button::PressedCallback::Callback callback)
    : callback_(std::move(callback)) {}

Button::PressedCallback::PressedCallback(base::RepeatingClosure closure)
    : callback_(std::move(closure)) {}

Button::PressedCallback::PressedCallback(PressedCallback&&) = default;

Button::PressedCallback& Button::PressedCallback::operator=(PressedCallback&&) =
    default;

Button::PressedCallback::~PressedCallback() = default;

Button::PressedCallback::operator bool() const {
  return absl::visit([](const auto& callback) { return !callback.is_null(); },
                     callback_);
}

void Button::PressedCallback::Run(const ui::Event& event) {
  return absl::visit(
      base::Overloaded{
          [](base::OnceClosure& closure) { std::move(closure).Run(); },
          [](const base::RepeatingClosure& closure) { closure.Run(); },
          [&](const Callback& callback) { callback.Run(event); },
      },
      callback_);
}

Button::ScopedAnchorHighlight::ScopedAnchorHighlight(
    base::WeakPtr<Button> button)
    : button_(std::move(button)) {}
Button::ScopedAnchorHighlight::~ScopedAnchorHighlight() {
  if (button_) {
    button_->ReleaseAnchorHighlight();
  }
}
Button::ScopedAnchorHighlight::ScopedAnchorHighlight(
    Button::ScopedAnchorHighlight&&) = default;

// We need to implement this one manually because the default move assignment
// operator does not call the destructor on `this`. That leads to us failing to
// release our reference on `button_`.
Button::ScopedAnchorHighlight& Button::ScopedAnchorHighlight::operator=(
    Button::ScopedAnchorHighlight&& other) {
  if (button_) {
    button_->ReleaseAnchorHighlight();
  }

  button_ = std::move(other.button_);
  return *this;
}

// static
constexpr Button::ButtonState Button::kButtonStates[STATE_COUNT];

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
    case ui::NativeTheme::kDisabled:
      return Button::STATE_DISABLED;
    case ui::NativeTheme::kHovered:
      return Button::STATE_HOVERED;
    case ui::NativeTheme::kNormal:
      return Button::STATE_NORMAL;
    case ui::NativeTheme::kPressed:
      return Button::STATE_PRESSED;
    case ui::NativeTheme::kNumStates:
      NOTREACHED();
  }
  return Button::STATE_NORMAL;
}

Button::~Button() = default;

void Button::SetTooltipText(const std::u16string& tooltip_text) {
  if (tooltip_text == tooltip_text_) {
    return;
  }

  if (GetViewAccessibility().GetCachedName().empty() ||
      GetViewAccessibility().GetCachedName() == tooltip_text_) {
    GetViewAccessibility().SetName(tooltip_text);
  }

  tooltip_text_ = tooltip_text;
  OnSetTooltipText(tooltip_text);
  TooltipTextChanged();
  OnPropertyChanged(&tooltip_text_, kPropertyEffectsNone);
}

const std::u16string& Button::GetTooltipText() const {
  return tooltip_text_;
}

void Button::SetCallback(PressedCallback callback) {
  callback_ = std::move(callback);
}

void Button::AdjustAccessibleName(std::u16string& new_name,
                                  ax::mojom::NameFrom& name_from) {
  if (new_name.empty()) {
    new_name = tooltip_text_;
  }
}

Button::ButtonState Button::GetState() const {
  return state_;
}

void Button::SetState(ButtonState state) {
  if (state == state_) {
    return;
  }

  if (animate_on_state_change_) {
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
  // The hover animation affects the highlight state, make sure the highlight
  // state is correct if there are supposed to be anchor highlights.
  if (anchor_count_ > 0) {
    SetHighlighted(true);
  }

  ButtonState old_state = state_;
  state_ = state;

  GetViewAccessibility().SetIsEnabled(state_ != STATE_DISABLED);
  GetViewAccessibility().SetIsHovered(state_ == STATE_HOVERED);
  UpdateAccessibleCheckedState();
  StateChanged(old_state);
  OnPropertyChanged(&state_, kPropertyEffectsPaint);
}

int Button::GetTag() const {
  return tag_;
}

void Button::SetTag(int tag) {
  if (tag_ == tag) {
    return;
  }
  tag_ = tag;
  OnPropertyChanged(&tag_, kPropertyEffectsNone);
}

void Button::SetAnimationDuration(base::TimeDelta duration) {
  hover_animation_.SetSlideDuration(duration);
}

void Button::SetTriggerableEventFlags(int triggerable_event_flags) {
  if (triggerable_event_flags == triggerable_event_flags_) {
    return;
  }
  triggerable_event_flags_ = triggerable_event_flags;
  OnPropertyChanged(&triggerable_event_flags_, kPropertyEffectsNone);
}

int Button::GetTriggerableEventFlags() const {
  return triggerable_event_flags_;
}

void Button::SetRequestFocusOnPress(bool value) {
// On Mac, buttons should not request focus on a mouse press. Hence keep the
// default value i.e. false.
#if !BUILDFLAG(IS_MAC)
  if (request_focus_on_press_ == value) {
    return;
  }
  request_focus_on_press_ = value;
  OnPropertyChanged(&request_focus_on_press_, kPropertyEffectsNone);
#endif
}

bool Button::GetRequestFocusOnPress() const {
  return request_focus_on_press_;
}

void Button::SetAnimateOnStateChange(bool value) {
  if (value == animate_on_state_change_) {
    return;
  }
  animate_on_state_change_ = value;
  OnPropertyChanged(&animate_on_state_change_, kPropertyEffectsNone);
}

bool Button::GetAnimateOnStateChange() const {
  return animate_on_state_change_;
}

void Button::SetHideInkDropWhenShowingContextMenu(bool value) {
  if (value == hide_ink_drop_when_showing_context_menu_) {
    return;
  }
  hide_ink_drop_when_showing_context_menu_ = value;
  OnPropertyChanged(&hide_ink_drop_when_showing_context_menu_,
                    kPropertyEffectsNone);
}

bool Button::GetHideInkDropWhenShowingContextMenu() const {
  return hide_ink_drop_when_showing_context_menu_;
}

void Button::SetShowInkDropWhenHotTracked(bool value) {
  if (value == show_ink_drop_when_hot_tracked_) {
    return;
  }
  show_ink_drop_when_hot_tracked_ = value;
  OnPropertyChanged(&show_ink_drop_when_hot_tracked_, kPropertyEffectsNone);
}

bool Button::GetShowInkDropWhenHotTracked() const {
  return show_ink_drop_when_hot_tracked_;
}

void Button::SetHasInkDropActionOnClick(bool value) {
  if (value == has_ink_drop_action_on_click_) {
    return;
  }
  has_ink_drop_action_on_click_ = value;
  OnPropertyChanged(&has_ink_drop_action_on_click_, kPropertyEffectsNone);
}

bool Button::GetHasInkDropActionOnClick() const {
  return has_ink_drop_action_on_click_;
}

void Button::SetInstallFocusRingOnFocus(bool install) {
  if (install == GetInstallFocusRingOnFocus()) {
    return;
  }
  if (install) {
    FocusRing::Install(this);
  } else {
    FocusRing::Remove(this);
  }
}

bool Button::GetInstallFocusRingOnFocus() const {
  return FocusRing::Get(this) != nullptr;
}

void Button::SetHotTracked(bool is_hot_tracked) {
  if (state_ != STATE_DISABLED) {
    SetState(is_hot_tracked ? STATE_HOVERED : STATE_NORMAL);
    if (show_ink_drop_when_hot_tracked_) {
      InkDrop::Get(ink_drop_view_)
          ->AnimateToState(is_hot_tracked ? views::InkDropState::ACTIVATED
                                          : views::InkDropState::HIDDEN,
                           nullptr);
    }
  }

  if (is_hot_tracked) {
    NotifyAccessibilityEvent(ax::mojom::Event::kHover, true);
  }
}

bool Button::IsHotTracked() const {
  return state_ == STATE_HOVERED;
}

void Button::SetFocusPainter(std::unique_ptr<Painter> focus_painter) {
  focus_painter_ = std::move(focus_painter);
}

void Button::SetHighlighted(bool highlighted) {
  // Do nothing if the ink drop's target state matches what we are trying to set
  // since same state transitions may restart animations.
  InkDropState state = highlighted ? views::InkDropState::ACTIVATED
                                   : views::InkDropState::DEACTIVATED;
  if (InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() ==
      state) {
    return;
  }
  InkDrop::Get(ink_drop_view_)->AnimateToState(state, nullptr);
}

Button::ScopedAnchorHighlight Button::AddAnchorHighlight() {
  if (0 == anchor_count_++) {
    SetHighlighted(true);
  }
  anchor_count_changed_callbacks_.Notify(anchor_count_);
  return ScopedAnchorHighlight(GetWeakPtr());
}

base::CallbackListSubscription Button::AddStateChangedCallback(
    PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&state_, std::move(callback));
}

base::CallbackListSubscription Button::AddAnchorCountChangedCallback(
    base::RepeatingCallback<void(size_t)> callback) {
  return anchor_count_changed_callbacks_.Add(std::move(callback));
}

Button::KeyClickAction Button::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return PlatformStyle::kKeyClickActionOnSpace;
  // Note that default buttons also have VKEY_RETURN installed as an accelerator
  // in LabelButton::SetIsDefault(). On platforms where
  // PlatformStyle::kReturnClicksFocusedControl, the logic here will take
  // precedence over that.
  if (event.key_code() == ui::VKEY_RETURN &&
      PlatformStyle::kReturnClicksFocusedControl)
    return KeyClickAction::kOnKeyPress;
  return KeyClickAction::kNone;
}

void Button::SetButtonController(
    std::unique_ptr<ButtonController> button_controller) {
  button_controller_ = std::move(button_controller);
  UpdateAccessibleDefaultActionVerb();
}

gfx::Point Button::GetMenuPosition() const {
  gfx::Rect lb = GetLocalBounds();

  // Offset of the associated menu position.
  constexpr gfx::Vector2d kMenuOffset{-2, -4};

  // The position of the menu depends on whether or not the locale is
  // right-to-left.
  gfx::Point menu_position(lb.right(), lb.bottom());
  if (base::i18n::IsRTL()) {
    menu_position.set_x(lb.x());
  }

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

void Button::SetInkDropView(View* view) {
  if (ink_drop_view_ == view) {
    return;
  }

  InkDrop::Remove(ink_drop_view_);
  ink_drop_view_ = view;
}

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
      if (should_show_pending &&
          InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() ==
              views::InkDropState::HIDDEN) {
        InkDrop::Get(ink_drop_view_)
            ->AnimateToState(views::InkDropState::ACTION_PENDING, &event);
      }
    } else {
      SetState(STATE_NORMAL);
      if (should_show_pending &&
          InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() ==
              views::InkDropState::ACTION_PENDING) {
        InkDrop::Get(ink_drop_view_)
            ->AnimateToState(views::InkDropState::HIDDEN, &event);
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
  if (state_ != STATE_DISABLED) {
    SetState(STATE_NORMAL);
  }
  InkDrop::Get(ink_drop_view_)
      ->AnimateToState(views::InkDropState::HIDDEN, nullptr /* event */);
  InkDrop::Get(ink_drop_view_)->GetInkDrop()->SetHovered(false);
  View::OnMouseCaptureLost();
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

std::u16string Button::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
}

void Button::ShowContextMenu(const gfx::Point& p,
                             ui::MenuSourceType source_type) {
  if (!context_menu_controller()) {
    return;
  }

  // We're about to show the context menu. Showing the context menu likely means
  // we won't get a mouse exited and reset state. Reset it now to be sure.
  if (state_ != STATE_DISABLED) {
    SetState(STATE_NORMAL);
  }
  if (hide_ink_drop_when_showing_context_menu_) {
    InkDrop::Get(ink_drop_view_)->GetInkDrop()->SetHovered(false);
    InkDrop::Get(ink_drop_view_)
        ->AnimateToState(InkDropState::HIDDEN, nullptr /* event */);
  }
  View::ShowContextMenu(p, source_type);
}

void Button::OnDragDone() {
  // Only reset the state to normal if the button isn't currently disabled
  // (since disabled buttons may still be able to be dragged).
  if (state_ != STATE_DISABLED) {
    SetState(STATE_NORMAL);
  }
  if (anchor_count_ > 0) {
    SetHighlighted(true);
  } else {
    InkDrop::Get(ink_drop_view_)
        ->AnimateToState(InkDropState::HIDDEN, nullptr /* event */);
  }
}

void Button::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  PaintButtonContents(canvas);
  Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void Button::VisibilityChanged(View* starting_from, bool visible) {
  View::VisibilityChanged(starting_from, visible);
  if (state_ == STATE_DISABLED) {
    return;
  }
  SetState(visible && ShouldEnterHoveredState() ? STATE_HOVERED : STATE_NORMAL);
  if (visible && anchor_count_ > 0) {
    SetHighlighted(true);
  }
}

void Button::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  if (!details.is_add && state_ != STATE_DISABLED && details.child == this)
    SetState(STATE_NORMAL);
  View::ViewHierarchyChanged(details);
}

void Button::OnFocus() {
  View::OnFocus();
  if (focus_painter_) {
    SchedulePaint();
  }
}

void Button::OnBlur() {
  View::OnBlur();
  if (IsHotTracked() || state_ == STATE_PRESSED) {
    SetState(STATE_NORMAL);
    if (InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() !=
        views::InkDropState::HIDDEN) {
      InkDrop::Get(ink_drop_view_)
          ->AnimateToState(views::InkDropState::HIDDEN, nullptr /* event */);
    }
    // TODO(bruthig) : Fix Buttons to work well when multiple input
    // methods are interacting with a button. e.g. By animating to HIDDEN here
    // it is possible for a Mouse Release to trigger an action however there
    // would be no visual cue to the user that this will occur.
  }
  if (focus_painter_) {
    SchedulePaint();
  }
}

std::unique_ptr<ActionViewInterface> Button::GetActionViewInterface() {
  return std::make_unique<ButtonActionViewInterface>(this);
}

void Button::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

Button::Button(PressedCallback callback)
    : AnimationDelegateViews(this), callback_(std::move(callback)) {
  InkDrop::Install(this, std::make_unique<InkDropHost>(this));

  SetFocusBehavior(PlatformStyle::kDefaultFocusBehavior);
  SetProperty(kIsButtonProperty, true);
  hover_animation_.SetSlideDuration(base::Milliseconds(150));
  SetInstallFocusRingOnFocus(true);
  button_controller_ = std::make_unique<ButtonController>(
      this, std::make_unique<DefaultButtonControllerDelegate>(this));
  InkDrop::Get(ink_drop_view_)
      ->SetCreateInkDropCallback(base::BindRepeating(
          [](Button* button) {
            std::unique_ptr<InkDrop> ink_drop =
                InkDrop::CreateInkDropForFloodFillRipple(InkDrop::Get(button));
            ink_drop->SetShowHighlightOnFocus(!FocusRing::Get(button));
            return ink_drop;
          },
          base::Unretained(this)));
  // TODO(pbos): Investigate not setting a default color so that we can DCHECK
  // if one hasn't been set.
  InkDrop::Get(ink_drop_view_)->SetBaseColor(gfx::kPlaceholderColor);

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  UpdateAccessibleDefaultActionVerb();
}

void Button::RequestFocusFromEvent() {
  if (request_focus_on_press_) {
    RequestFocus();
  }
}

void Button::NotifyClick(const ui::Event& event) {
  if (has_ink_drop_action_on_click_) {
    InkDrop::Get(ink_drop_view_)
        ->AnimateToState(InkDropState::ACTION_TRIGGERED,
                         ui::LocatedEvent::FromIfValid(&event));
  }

  // If we have an associated help context ID, notify that system that we have
  // been activated.
  const ui::ElementIdentifier element_id = GetProperty(kElementIdentifierKey);
  if (element_id) {
    views::ElementTrackerViews::GetInstance()->NotifyViewActivated(element_id,
                                                                   this);
  }

  if (callback_) {
    callback_.Run(event);
  }
}

void Button::OnClickCanceled(const ui::Event& event) {
  if (ShouldUpdateInkDropOnClickCanceled()) {
    if (InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() ==
            views::InkDropState::ACTION_PENDING ||
        InkDrop::Get(ink_drop_view_)->GetInkDrop()->GetTargetInkDropState() ==
            views::InkDropState::ALTERNATE_ACTION_PENDING) {
      InkDrop::Get(ink_drop_view_)
          ->AnimateToState(views::InkDropState::HIDDEN,
                           ui::LocatedEvent::FromIfValid(&event));
    }
  }
}

void Button::OnSetTooltipText(const std::u16string& tooltip_text) {}

void Button::StateChanged(ButtonState old_state) {}

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
  if (!GetVisible()) {
    return false;
  }

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

base::WeakPtr<Button> Button::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Button::OnEnabledChanged() {
  if (GetEnabled() ? (state_ != STATE_DISABLED) : (state_ == STATE_DISABLED))
    return;

  if (GetEnabled()) {
    bool should_enter_hover_state = ShouldEnterHoveredState();
    SetState(should_enter_hover_state ? STATE_HOVERED : STATE_NORMAL);
    InkDrop::Get(ink_drop_view_)
        ->GetInkDrop()
        ->SetHovered(should_enter_hover_state);
  } else {
    SetState(STATE_DISABLED);
    InkDrop::Get(ink_drop_view_)->GetInkDrop()->SetHovered(false);
  }
  UpdateAccessibleDefaultActionVerb();
}

void Button::ReleaseAnchorHighlight() {
  if (0 == --anchor_count_) {
    SetHighlighted(false);
  }
  anchor_count_changed_callbacks_.Notify(anchor_count_);
}

void Button::UpdateAccessibleCheckedState() {
  switch (state_) {
    case STATE_PRESSED:
      GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kTrue);
      break;
    default:
      GetViewAccessibility().RemoveCheckedState();
      break;
  }
}

void Button::SetDefaultActionVerb(ax::mojom::DefaultActionVerb verb) {
  default_action_verb_ = verb;
}

void Button::UpdateAccessibleDefaultActionVerb() {
  if (GetEnabled()) {
    GetViewAccessibility().SetDefaultActionVerb(default_action_verb_);
  } else {
    GetViewAccessibility().RemoveDefaultActionVerb();
  }

  if (button_controller_) {
    button_controller_->UpdateButtonAccessibleDefaultActionVerb();
  }
}

ButtonActionViewInterface::ButtonActionViewInterface(Button* action_view)
    : BaseActionViewInterface(action_view), action_view_(action_view) {}

void ButtonActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  BaseActionViewInterface::ActionItemChangedImpl(action_item);
  std::u16string tooltip_text = action_item->GetTooltipText();
  if (!tooltip_text.empty()) {
    action_view_->SetTooltipText(tooltip_text);
  }
}

void ButtonActionViewInterface::LinkActionInvocationToView(
    base::RepeatingClosure invoke_action_callback) {
  if (!action_view_) {
    return;
  }
  action_view_->SetCallback(invoke_action_callback);
}

BEGIN_METADATA(Button)
ADD_PROPERTY_METADATA(PressedCallback, Callback)
ADD_PROPERTY_METADATA(bool, AnimateOnStateChange)
ADD_PROPERTY_METADATA(bool, HasInkDropActionOnClick)
ADD_PROPERTY_METADATA(bool, HideInkDropWhenShowingContextMenu)
ADD_PROPERTY_METADATA(bool, InstallFocusRingOnFocus)
ADD_PROPERTY_METADATA(bool, RequestFocusOnPress)
ADD_PROPERTY_METADATA(ButtonState, State)
ADD_PROPERTY_METADATA(int, Tag)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
ADD_PROPERTY_METADATA(int, TriggerableEventFlags)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(views::Button::ButtonState,
                       {views::Button::STATE_NORMAL, u"STATE_NORMAL"},
                       {views::Button::STATE_HOVERED, u"STATE_HOVERED"},
                       {views::Button::STATE_PRESSED, u"STATE_PRESSED"},
                       {views::Button::STATE_DISABLED, u"STATE_DISABLED"})
