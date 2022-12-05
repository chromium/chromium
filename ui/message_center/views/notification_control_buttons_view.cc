// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_button_factory.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

NotificationControlButtonsView::NotificationControlButtonsView(
    MessageView* message_view)
    : message_view_(message_view) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // Do not stretch buttons as that would stretch their focus indicator.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Use layer to change the opacity.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (!notification_control_button_factory_) {
    notification_control_button_factory_ =
        std::make_unique<NotificationControlButtonFactory>();
  }
}

NotificationControlButtonsView::~NotificationControlButtonsView() = default;

void NotificationControlButtonsView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateButtonIconColors();
}

void NotificationControlButtonsView::ShowCloseButton(bool show) {
  if (show && !close_button_) {
    close_button_ =
        AddChildView(notification_control_button_factory_->CreateButton(
            base::BindRepeating(&MessageView::OnCloseButtonPressed,
                                base::Unretained(message_view_))));
    if (GetWidget()) {
      close_button_->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kNotificationCloseButtonIcon,
                                         DetermineButtonIconColor()));
    }
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && close_button_) {
    DCHECK(Contains(close_button_));
    RemoveChildViewT(close_button_.get());
    close_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    // Add the button next right to the snooze button.
    const int position = snooze_button_ ? 1 : 0;
    settings_button_ = AddChildViewAt(
        notification_control_button_factory_->CreateButton(
            base::BindRepeating(&MessageView::OnSettingsButtonPressed,
                                base::Unretained(message_view_))),
        position);
    if (GetWidget()) {
      settings_button_->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kNotificationSettingsButtonIcon,
                                         DetermineButtonIconColor()));
    }
    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && settings_button_) {
    DCHECK(Contains(settings_button_));
    RemoveChildViewT(settings_button_.get());
    settings_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowSnoozeButton(bool show) {
  if (show && !snooze_button_) {
    // Snooze button should appear as the first child.
    snooze_button_ = AddChildViewAt(
        notification_control_button_factory_->CreateButton(
            base::BindRepeating(&MessageView::OnSnoozeButtonPressed,
                                base::Unretained(message_view_))),
        0);
    if (GetWidget()) {
      snooze_button_->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kNotificationSnoozeButtonIcon,
                                         DetermineButtonIconColor()));
    }
    snooze_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP));
    snooze_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP));
    snooze_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && snooze_button_) {
    DCHECK(Contains(snooze_button_));
    RemoveChildViewT(snooze_button_.get());
    snooze_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowButtons(bool show) {
  DCHECK(layer());
  // Manipulate the opacity instead of changing the visibility to keep the tab
  // order even when the view is invisible.
  layer()->SetOpacity(show ? 1. : 0.);
  SetCanProcessEventsWithinSubtree(show);
}

bool NotificationControlButtonsView::IsAnyButtonFocused() const {
  return (close_button_ && close_button_->HasFocus()) ||
         (settings_button_ && settings_button_->HasFocus()) ||
         (snooze_button_ && snooze_button_->HasFocus());
}

void NotificationControlButtonsView::SetButtonIconColors(SkColor color) {
  if (color == icon_color_)
    return;
  icon_color_ = color;
  if (GetWidget())
    UpdateButtonIconColors();
}

void NotificationControlButtonsView::SetBackgroundColor(SkColor color) {
  if (color == background_color_)
    return;
  background_color_ = color;
  UpdateButtonIconColors();
}

void NotificationControlButtonsView::SetMessageView(MessageView* message_view) {
  message_view_ = message_view;
}

void NotificationControlButtonsView::SetNotificationControlButtonFactory(
    std::unique_ptr<NotificationControlButtonFactory>
        notification_control_button_factory) {
  notification_control_button_factory_ =
      std::move(notification_control_button_factory);
}

void NotificationControlButtonsView::UpdateButtonIconColors() {
  SkColor icon_color = DetermineButtonIconColor();
  if (close_button_) {
    close_button_->SetImageModel(views::Button::STATE_NORMAL,
                                 ui::ImageModel::FromVectorIcon(
                                     kNotificationCloseButtonIcon, icon_color));
  }
  if (settings_button_) {
    settings_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kNotificationSettingsButtonIcon,
                                       icon_color));
  }
  if (snooze_button_) {
    snooze_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kNotificationSnoozeButtonIcon,
                                       icon_color));
  }
}

SkColor NotificationControlButtonsView::DetermineButtonIconColor() const {
  const SkColor icon_color =
      icon_color_.value_or(GetColorProvider()->GetColor(ui::kColorIcon));
  if (SkColorGetA(background_color_) != SK_AlphaOPAQUE)
    return icon_color;

  return color_utils::BlendForMinContrast(icon_color, background_color_).color;
}

BEGIN_METADATA(NotificationControlButtonsView, views::View)
END_METADATA

}  // namespace message_center
