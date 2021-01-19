// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace message_center {

namespace {

// Creates a text for spoken feedback from the data contained in the
// notification.
base::string16 CreateAccessibleName(const Notification& notification) {
  if (!notification.accessible_name().empty())
    return notification.accessible_name();

  // Fall back to a text constructed from the notification.
  // Add non-empty elements.

  std::vector<base::string16> accessible_lines;
  if (!notification.title().empty())
    accessible_lines.push_back(notification.title());

  if (!notification.message().empty())
    accessible_lines.push_back(notification.message());

  if (!notification.context_message().empty())
    accessible_lines.push_back(notification.context_message());
  std::vector<NotificationItem> items = notification.items();
  for (size_t i = 0; i < items.size() && i < kNotificationMaximumItems; ++i) {
    accessible_lines.push_back(items[i].title + base::ASCIIToUTF16(" ") +
                               items[i].message);
  }
  return base::JoinString(accessible_lines, base::ASCIIToUTF16("\n"));
}

bool ShouldShowAeroShadowBorder() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return false;
#endif
}

}  // namespace

// static
const char MessageView::kViewClassName[] = "MessageView";

MessageView::HighlightPathGenerator::HighlightPathGenerator() = default;

SkPath MessageView::HighlightPathGenerator::GetHighlightPath(
    const views::View* view) {
  return static_cast<const MessageView*>(view)->GetHighlightPath();
}

MessageView::MessageView(const Notification& notification)
    : notification_id_(notification.id()), slide_out_controller_(this, this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  focus_ring_ = views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>());

  // Paint to a dedicated layer to make the layer non-opaque.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateWithNotification(notification);

  UpdateCornerRadius(0, 0);

  // If Aero is enabled, set shadow border.
  if (ShouldShowAeroShadowBorder()) {
    const auto& shadow = gfx::ShadowDetails::Get(2, 0);
    gfx::Insets ninebox_insets = gfx::ShadowValue::GetBlurRegion(shadow.values);
    SetBorder(views::CreateBorderPainter(
        views::Painter::CreateImagePainter(shadow.ninebox_image,
                                           ninebox_insets),
        -gfx::ShadowValue::GetMargin(shadow.values)));
  }
}

MessageView::~MessageView() {
  RemovedFromWidget();
}

void MessageView::UpdateWithNotification(const Notification& notification) {
  pinned_ = notification.pinned();
  base::string16 new_accessible_name = CreateAccessibleName(notification);
  if (new_accessible_name != accessible_name_) {
    accessible_name_ = new_accessible_name;
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  }
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
}

void MessageView::SetIsNested() {
  DCHECK(!is_nested_) << "MessageView::SetIsNested() is called twice wrongly.";

  is_nested_ = true;
  // Update enability since it might be changed by "is_nested" flag.
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
  slide_out_controller_.set_update_opacity(false);

  SkColor border_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_UnfocusedBorderColor);
  SetBorder(views::CreateRoundedRectBorder(
      kNotificationBorderThickness, kNotificationCornerRadius, border_color));

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
  // Not implemented by default.
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

void MessageView::SetManuallyExpandedOrCollapsed(bool value) {
  // Not implemented by default.
}

void MessageView::UpdateCornerRadius(int top_radius, int bottom_radius) {
  SetCornerRadius(top_radius, bottom_radius);
  UpdateBackgroundPainter();
  SchedulePaint();
}

SkPath MessageView::GetHighlightPath() const {
  gfx::Rect rect(GetBoundsInScreen().size());
  // Shrink focus ring size by -kFocusHaloInset on each side to draw
  // them on top of the notifications. We need to do this because TrayBubbleView
  // has a layer that masks to bounds due to which the focus ring can not extend
  // outside the view.
  int inset = -views::PlatformStyle::kFocusHaloInset;
  rect.Inset(gfx::Insets(inset));

  int top_radius = std::max(0, top_radius_ - inset);
  int bottom_radius = std::max(0, bottom_radius_ - inset);
  SkScalar radii[8] = {top_radius,    top_radius,      // top-left
                       top_radius,    top_radius,      // top-right
                       bottom_radius, bottom_radius,   // bottom-right
                       bottom_radius, bottom_radius};  // bottom-left

  return SkPath().addRoundRect(gfx::RectToSkRect(rect), radii);
}

void MessageView::OnContainerAnimationStarted() {
  // Not implemented by default.
}

void MessageView::OnContainerAnimationEnded() {
  // Not implemented by default.
}

void MessageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_MESSAGE_NOTIFICATION_ACCESSIBLE_NAME));

  if (accessible_name_.empty())
    node_data->SetNameFrom(ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  node_data->SetName(accessible_name_);
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

bool MessageView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void MessageView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return;

  MessageCenter::Get()->ClickOnNotification(notification_id_);
}

bool MessageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.flags() != ui::EF_NONE)
    return false;

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

const char* MessageView::GetClassName() const {
  return kViewClassName;
}

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      SetDrawBackgroundAsActive(true);
      break;
    }
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END: {
      SetDrawBackgroundAsActive(false);
      break;
    }
    case ui::ET_GESTURE_TAP: {
      SetDrawBackgroundAsActive(false);
      MessageCenter::Get()->ClickOnNotification(notification_id_);
      event->SetHandled();
      return;
    }
    default: {
      // Do nothing
    }
  }

  if (!event->IsScrollGestureEvent() && !event->IsFlingScrollEvent())
    return;

  if (scroller_)
    scroller_->OnGestureEvent(event);
  event->SetHandled();
}

void MessageView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void MessageView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void MessageView::OnThemeChanged() {
  InkDropHostView::OnThemeChanged();
  UpdateBackgroundPainter();
}

ui::Layer* MessageView::GetSlideOutLayer() {
  return is_nested_ ? layer() : GetWidget()->GetLayer();
}

void MessageView::OnSlideStarted() {
  for (auto& observer : observers_) {
    observer.OnSlideStarted(notification_id_);
  }
}

void MessageView::OnSlideChanged(bool in_progress) {
  for (auto& observer : observers_) {
    observer.OnSlideChanged(notification_id_);
  }
}

void MessageView::AddObserver(MessageView::Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageView::RemoveObserver(MessageView::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessageView::OnSlideOut() {
  // The notification will be deleted after slide out, so give observers a
  // chance to handle the notification before fulling sliding out.
  for (auto& observer : observers_)
    observer.OnPreSlideOut(notification_id_);

  // Copy the |notification_id| here as calling OnSlideOut() might destroy
  // |this| but we still want to call RemoveNotification(). Note that the
  // iteration over |observers_| is still safe and will simply stop.
  std::string notification_id_copy = notification_id_;

  for (auto& observer : observers_)
    observer.OnSlideOut(notification_id_);

  MessageCenter::Get()->RemoveNotification(notification_id_copy,
                                           true /* by_user */);
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
  if (disable_slide_)
    return views::SlideOutController::SlideMode::kNone;

  switch (GetMode()) {
    case Mode::SETTING:
      return views::SlideOutController::SlideMode::kNone;
    case Mode::PINNED:
      return views::SlideOutController::SlideMode::kPartial;
    case Mode::NORMAL:
      return views::SlideOutController::SlideMode::kFull;
  }

  NOTREACHED();
  return views::SlideOutController::SlideMode::kFull;
}

MessageView::Mode MessageView::GetMode() const {
  if (setting_mode_)
    return Mode::SETTING;

  // Only nested notifications can be pinned. Standalones (i.e. popups) can't
  // be.
  if (pinned_ && is_nested_)
    return Mode::PINNED;

  return Mode::NORMAL;
}

float MessageView::GetSlideAmount() const {
  return slide_out_controller_.gesture_amount();
}

void MessageView::SetSettingMode(bool setting_mode) {
  setting_mode_ = setting_mode;
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
  UpdateControlButtonsVisibility();
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
}

void MessageView::OnCloseButtonPressed() {
  for (auto& observer : observers_)
    observer.OnCloseButtonPressed(notification_id_);
  MessageCenter::Get()->RemoveNotification(notification_id_,
                                           true /* by_user */);
}

void MessageView::OnSettingsButtonPressed(const ui::Event& event) {
  for (auto& observer : observers_)
    observer.OnSettingsButtonPressed(notification_id_);

  MessageCenter::Get()->ClickOnSettingsButton(notification_id_);
}

void MessageView::OnSnoozeButtonPressed(const ui::Event& event) {
  for (auto& observer : observers_)
    observer.OnSnoozeButtonPressed(notification_id_);
}

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

void MessageView::UpdateBackgroundPainter() {
  SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationDefaultBackground);
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(
          top_radius_, bottom_radius_, background_color)));
}

void MessageView::UpdateControlButtonsVisibility() {
  auto* control_buttons_view = GetControlButtonsView();
  if (control_buttons_view)
    control_buttons_view->ShowButtons(ShouldShowControlButtons());
}

void MessageView::SetDrawBackgroundAsActive(bool active) {
  background()->SetNativeControlColor(active ? kHoveredButtonBackgroundColor
                                             : kNotificationBackgroundColor);
  SchedulePaint();
}

}  // namespace message_center
