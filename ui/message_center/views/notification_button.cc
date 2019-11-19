// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_button.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::Button(listener), icon_(NULL), title_(NULL) {
  SetFocusForPlatform();
  // Create a background so that it does not change when the MessageView
  // background changes to show touch feedback
  SetBackground(views::CreateSolidBackground(kNotificationBackgroundColor));
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kButtonHorizontalPadding), kButtonIconToTitlePadding));
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 2, 2, 2)));
}

NotificationButton::~NotificationButton() {
}

void NotificationButton::SetIcon(const gfx::ImageSkia& image) {
  if (icon_ != NULL)
    delete icon_;  // This removes the icon from this view's children.
  if (image.isNull()) {
    icon_ = NULL;
  } else {
    icon_ = new views::ImageView();
    icon_->SetImageSize(
        gfx::Size(kNotificationButtonIconSize, kNotificationButtonIconSize));
    icon_->SetImage(image);
    icon_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
    icon_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
    icon_->SetBorder(views::CreateEmptyBorder(kButtonIconTopPadding, 0, 0, 0));
    AddChildViewAt(icon_, 0);
  }
}

void NotificationButton::SetTitle(const base::string16& title) {
  if (title_)
    delete title_;  // This removes the title from this view's children.
  title_ = nullptr;
  if (!title.empty()) {
    title_ = new views::Label(title);
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetEnabledColor(kRegularTextColor);
    title_->SetAutoColorReadabilityEnabled(false);
    AddChildView(title_);
  }
  SetAccessibleName(title);
}

gfx::Size NotificationButton::CalculatePreferredSize() const {
  return gfx::Size(kNotificationWidth, kButtonHeight);
}

int NotificationButton::GetHeightForWidth(int width) const {
  return kButtonHeight;
}

void NotificationButton::OnFocus() {
  views::Button::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void NotificationButton::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // We disable view hierarchy change detection in the parent
  // because it resets the hoverstate, which we do not want
  // when we update the view to contain a new label or image.
  views::View::ViewHierarchyChanged(details);
}

void NotificationButton::StateChanged(ButtonState old_state) {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    SetBackground(views::CreateSolidBackground(kHoveredButtonBackgroundColor));
  } else {
    SetBackground(views::CreateSolidBackground(kNotificationBackgroundColor));
  }
}

}  // namespace message_center
