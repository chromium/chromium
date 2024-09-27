// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/time/time.h"
#endif

namespace message_center {

namespace {

bool ShouldShowAeroShadowBorder() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

// Helper function to setup focus ring shapes for `MessageView`.
void InstallHighlightPathGenerator(views::View* view,
                                   float top_radius,
                                   float bottom_radius) {
  auto corners = gfx::RoundedCornersF{top_radius, top_radius, bottom_radius,
                                      bottom_radius};
  // Shrink focus ring size by -`kDefaultHaloInset` on each side to draw
  // them on top of the notifications. We need to do this because
  // `TrayBubbleView` and `MessagePopupView` has a layer that masks to bounds
  // due to which the focus ring can not extend outside the view.
  views::HighlightPathGenerator::Install(
      view, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(-views::FocusRing::kDefaultHaloInset), corners));
}

}  // namespace

MessageView::MessageView(const Notification& notification)
    : notification_id_(notification.id()),
      notifier_id_(notification.notifier_id()),
      timestamp_(notification.timestamp()),
      slide_out_controller_(this, this) {
  SetNotifyEnterExitOnChild(true);
  slide_out_controller_.set_trackpad_gestures_enabled(true);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  // Paint to a dedicated layer to make the layer non-opaque.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  GetViewAccessibility().SetRoleDescription(
      l10n_util::GetStringUTF8(IDS_MESSAGE_NOTIFICATION_ACCESSIBLE_NAME));

  UpdateWithNotification(notification);

  UpdateCornerRadius(0, 0);

  // If Aero is enabled, set shadow border.
  if (ShouldShowAeroShadowBorder()) {
    const auto& shadow = gfx::ShadowDetails::Get(2, 0);
    gfx::Insets ninebox_insets = gfx::ShadowValue::GetBlurRegion(shadow.values);
    SetBorder(views::CreateBorderPainter(
        views::Painter::CreateImagePainter(shadow.nine_patch_image,
                                           ninebox_insets),
        -gfx::ShadowValue::GetMargin(shadow.values)));
  }
}

MessageView::~MessageView() {
  RemovedFromWidget();
}

views::View* MessageView::FindGroupNotificationView(
    const std::string& notification_id) {
  // Not implemented by default.
  return nullptr;
}

// Creates text for spoken feedback from the data contained in the
// notification.
std::u16string MessageView::CreateAccessibleName(
    const Notification& notification) {
  if (!notification.accessible_name().empty())
    return notification.accessible_name();

  // Fall back to text constructed from the notification.
  // Add non-empty elements.

  std::vector<std::u16string> accessible_lines;
  if (!notification.title().empty())
    accessible_lines.push_back(notification.title());

  if (!notification.message().empty())
    accessible_lines.push_back(notification.message());

  if (!notification.context_message().empty())
    accessible_lines.push_back(notification.context_message());
  std::vector<NotificationItem> items = notification.items();
  for (size_t i = 0; i < items.size() && i < kNotificationMaximumItems; ++i) {
    accessible_lines.push_back(items[i].title() + u" " + items[i].message());
  }
  return base::JoinString(accessible_lines, u"\n");
}

void MessageView::UpdateWithNotification(const Notification& notification) {
  pinned_ = notification.pinned();

  std::u16string name = CreateAccessibleName(notification);
  if (name.empty()) {
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(name);
  }

  slide_out_controller_.set_slide_mode(CalculateSlideMode());
}

void MessageView::SetIsNested() {
  DCHECK(!is_nested_) << "MessageView::SetIsNested() is called twice wrongly.";

  is_nested_ = true;
  // Update enability since it might be changed by "is_nested" flag.
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
  slide_out_controller_.set_update_opacity(false);

  UpdateNestedBorder();

  if (GetControlButtonsView())
    GetControlButtonsView()->ShowCloseButton(GetMode() != Mode::PINNED);
}

void MessageView::CloseSwipeControl() {
  slide_out_controller_.CloseSwipeControl();
}

void MessageView::SlideOutAndClose(int direction) {
  // Do not process events once the message view is animating out.
  // crbug.com/940719
  SetEnabled(false);

  slide_out_controller_.SlideOutAndClose(direction);
}

void MessageView::SetExpanded(bool expanded) {
  MessageCenter::Get()->OnSetExpanded(notification_id_, expanded);
}

bool MessageView::IsExpanded() const {
  // Not implemented by default.
  return false;
}

bool MessageView::IsAutoExpandingAllowed() const {
  // Allowed by default.
  return true;
}

bool MessageView::IsManuallyExpandedOrCollapsed() const {
  // Not implemented by default.
  return false;
}

void MessageView::SetManuallyExpandedOrCollapsed(ExpandState state) {
  // Not implemented by default.
}

void MessageView::ToggleInlineSettings(const ui::Event& event) {
  // Not implemented by default.
}

void MessageView::ToggleSnoozeSettings(const ui::Event& event) {
  // Not implemented by default.
}

void MessageView::UpdateCornerRadius(int top_radius, int bottom_radius) {
  SetCornerRadius(top_radius, bottom_radius);
  if (!GetWidget()) {
    return;
  }
  UpdateBackgroundPainter();
  SchedulePaint();
}

void MessageView::OnContainerAnimationStarted() {
  // Not implemented by default.
}

void MessageView::OnContainerAnimationEnded() {
  // Not implemented by default.
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

bool MessageView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void MessageView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return;
  }

  MessageCenter::Get()->ClickOnNotification(notification_id_);
}

void MessageView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateControlButtonsVisibility();
  MessageCenter::Get()->OnMessageViewHovered(notification_id_);
}

void MessageView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateControlButtonsVisibility();
}

bool MessageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.flags() != ui::EF_NONE) {
    return false;
  }

  if (event.key_code() == ui::VKEY_RETURN) {
    MessageCenter::Get()->ClickOnNotification(notification_id_);
    return true;
  } else if ((event.key_code() == ui::VKEY_DELETE ||
              event.key_code() == ui::VKEY_BACK)) {
    MessageCenter::Get()->RemoveNotification(notification_id_,
                                             true /* by_user */);
    return true;
  }

  return false;
}

bool MessageView::OnKeyReleased(const ui::KeyEvent& event) {
  // Space key handling is triggered at key-release timing. See
  // ui/views/controls/buttons/button.cc for why.
  if (event.flags() != ui::EF_NONE || event.key_code() != ui::VKEY_SPACE)
    return false;

  MessageCenter::Get()->ClickOnNotification(notification_id_);
  return true;
}

void MessageView::OnPaint(gfx::Canvas* canvas) {
  if (ShouldShowAeroShadowBorder()) {
    // If the border is shadow, paint border first.
    OnPaintBorder(canvas);
    // Clip at the border so we don't paint over it.
    canvas->ClipRect(GetContentsBounds());
    OnPaintBackground(canvas);
  } else {
    views::View::OnPaint(canvas);
  }
}

void MessageView::OnBlur() {
  views::View::OnBlur();
  // We paint a focus indicator.
  SchedulePaint();
}

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    MessageCenter::Get()->ClickOnNotification(notification_id_);
    event->SetHandled();
    return;
  }

  if (!event->IsScrollGestureEvent() && !event->IsFlingScrollEvent()) {
    return;
  }

  if (scroller_) {
    scroller_->OnGestureEvent(event);
  }
  event->SetHandled();
}

void MessageView::RemovedFromWidget() {
  if (!focus_manager_) {
    return;
  }
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void MessageView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }
}

void MessageView::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateNestedBorder();
  UpdateBackgroundPainter();
}

ui::Layer* MessageView::GetSlideOutLayer() {
  // If a message view is contained in a parent message view it should give up
  // slide behavior to the parent message view when the parent view is
  // collapsed.
  auto* nested_layer = (ShouldParentHandleSlide() && parent_message_view_)
                           ? parent_message_view_->layer()
                           : layer();
  bool is_nested = (ShouldParentHandleSlide() && parent_message_view_)
                       ? parent_message_view_->is_nested()
                       : is_nested_;
  return is_nested ? nested_layer : GetWidget()->GetLayer();
}

void MessageView::OnSlideStarted() {
  observers_.Notify(&Observer::OnSlideStarted, notification_id_);
}

void MessageView::OnSlideChanged(bool in_progress) {
  // crbug/1333664: We need to make sure to disable scrolling while a
  // notification view is sliding. This is to ensure the notification view can
  // only move horizontally or vertically at one time.
  if (scroller_ && !is_sliding_ && slide_out_controller_.GetGestureAmount()) {
    is_sliding_ = true;
    scroller_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
  }

  if (scroller_ && !in_progress) {
    is_sliding_ = false;
    scroller_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kEnabled);
  }

  if (in_progress) {
    observers_.Notify(&Observer::OnSlideChanged, notification_id_);
  } else {
    observers_.Notify(&Observer::OnSlideEnded, notification_id_);
  }
}

void MessageView::AddObserver(MessageView::Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageView::RemoveObserver(MessageView::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessageView::OnSlideOut() {
  if (ShouldParentHandleSlide() && parent_message_view_)
    return parent_message_view_->OnSlideOut();

  // The notification may be deleted after slide out, so give observers a
  // chance to handle the notification before fulling sliding out.
  observers_.Notify(&Observer::OnPreSlideOut, notification_id_);

  // Copy the |notification_id| here as calling OnSlideOut() might destroy
  // |this| but we still want to call RemoveNotification(). Note that the
  // iteration over |observers_| is still safe and will simply stop.
  std::string notification_id_copy = notification_id_;

  observers_.Notify(&Observer::OnSlideOut, notification_id_);

  auto* message_center = MessageCenter::Get();
  if (message_center->FindPopupNotificationById(notification_id_copy)) {
    message_center->MarkSinglePopupAsShown(notification_id_copy,
                                           /*mark_notification_as_read=*/true);
    return;
  }
  message_center->RemoveNotification(notification_id_copy, /*by_user=*/true);
}

void MessageView::OnWillChangeFocus(views::View* before, views::View* now) {}

void MessageView::OnDidChangeFocus(views::View* before, views::View* now) {
  if (Contains(before) || Contains(now) ||
      (GetControlButtonsView() && (GetControlButtonsView()->Contains(before) ||
                                   GetControlButtonsView()->Contains(now)))) {
    UpdateControlButtonsVisibility();
  }
}

views::SlideOutController::SlideMode MessageView::CalculateSlideMode() const {
  if (disable_slide_) {
    return views::SlideOutController::SlideMode::kNone;
  }

  switch (GetMode()) {
    case Mode::SETTING:
      return views::SlideOutController::SlideMode::kNone;
    case Mode::PINNED:
      return views::SlideOutController::SlideMode::kPartial;
    case Mode::NORMAL:
      return views::SlideOutController::SlideMode::kFull;
  }

  NOTREACHED_IN_MIGRATION();
  return views::SlideOutController::SlideMode::kFull;
}

MessageView::Mode MessageView::GetMode() const {
  if (setting_mode_) {
    return Mode::SETTING;
  }

  // Only nested notifications can be pinned. Standalones (i.e. popups) can't
  // be.
  if (pinned_ && is_nested_) {
    return Mode::PINNED;
  }

  return Mode::NORMAL;
}

float MessageView::GetSlideAmount() const {
  if (slide_out_controller_.mode() ==
      views::SlideOutController::SlideMode::kNone) {
    // The return value of this method is used by NotificationSwipeControlView
    // to determine visibility of the setting button. Returning 0 not to show
    // the setting button with SlideMode::kNone.
    return 0.f;
  }
  return slide_out_controller_.gesture_amount();
}

void MessageView::SetSettingMode(bool setting_mode) {
  setting_mode_ = setting_mode;
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
  UpdateControlButtonsVisibility();
}

void MessageView::DisableNotification() {
  MessageCenter::Get()->DisableNotification(notification_id());
}

void MessageView::DisableSlideForcibly(bool disable) {
  disable_slide_ = disable;
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
}

void MessageView::SetSlideButtonWidth(int control_button_width) {
  slide_out_controller_.SetSwipeControlWidth(control_button_width);
}

void MessageView::SetCornerRadius(int top_radius, int bottom_radius) {
  top_radius_ = top_radius;
  bottom_radius_ = bottom_radius;

  InstallHighlightPathGenerator(this, top_radius, bottom_radius);
}

void MessageView::OnCloseButtonPressed() {
  observers_.Notify(&Observer::OnCloseButtonPressed, notification_id_);
  MessageCenter::Get()->RemoveNotification(notification_id_,
                                           true /* by_user */);
}

void MessageView::OnSettingsButtonPressed(const ui::Event& event) {
  observers_.Notify(&Observer::OnSettingsButtonPressed, notification_id_);

  if (inline_settings_enabled_) {
    ToggleInlineSettings(event);
  } else {
    MessageCenter::Get()->ClickOnSettingsButton(notification_id_);
  }
}

void MessageView::OnSnoozeButtonPressed(const ui::Event& event) {
  observers_.Notify(&Observer::OnSnoozeButtonPressed, notification_id_);

  if (snooze_settings_enabled_) {
    ToggleSnoozeSettings(event);
  } else {
    MessageCenter::Get()->ClickOnSnoozeButton(notification_id());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::TimeDelta MessageView::GetBoundsAnimationDuration(
    const Notification& notification) const {
  return base::Milliseconds(0);
}
#endif

bool MessageView::ShouldShowControlButtons() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Users on ChromeOS are used to the Settings and Close buttons not being
  // visible at all times, but users on other platforms expect them to be
  // visible.
  auto* control_buttons_view = GetControlButtonsView();
  return control_buttons_view &&
         (control_buttons_view->IsAnyButtonFocused() ||
          (GetMode() != Mode::SETTING && IsMouseHovered()) ||
          MessageCenter::Get()->IsSpokenFeedbackEnabled());
#else
  return true;
#endif
}

bool MessageView::ShouldParentHandleSlide() const {
  if (!parent_message_view_) {
    return false;
  }

  return !parent_message_view_->IsExpanded();
}

void MessageView::UpdateBackgroundPainter() {
  const auto* color_provider = GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(ui::kColorNotificationBackgroundActive);

  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(
          top_radius_, bottom_radius_, background_color)));
}

void MessageView::UpdateNestedBorder() {
  if (!is_nested_ || !GetWidget()) {
    return;
  }

  SkColor border_color;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  border_color = SK_ColorTRANSPARENT;
#else
  border_color =
      GetColorProvider()->GetColor(ui::kColorFocusableBorderUnfocused);
#endif

  SetBorder(views::CreateRoundedRectBorder(
      kNotificationBorderThickness, kNotificationCornerRadius, border_color));
}

void MessageView::UpdateControlButtonsVisibility() {
  auto* control_buttons_view = GetControlButtonsView();
  if (control_buttons_view)
    control_buttons_view->ShowButtons(ShouldShowControlButtons());
}

void MessageView::UpdateControlButtonsVisibilityWithNotification(
    const Notification& notification) {
  auto* control_buttons_view = GetControlButtonsView();
  if (control_buttons_view) {
    control_buttons_view->ShowButtons(ShouldShowControlButtons());
    control_buttons_view->ShowSettingsButton(
        notification.should_show_settings_button());
    control_buttons_view->ShowSnoozeButton(
        notification.should_show_snooze_button());
    control_buttons_view->ShowCloseButton(GetMode() != Mode::PINNED);
  }
  UpdateControlButtonsVisibility();
}

BEGIN_METADATA(MessageView)
END_METADATA

}  // namespace message_center
