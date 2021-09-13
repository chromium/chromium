// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "ui/gfx/text_elider.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

namespace {

// TODO(crbug/1243889): Move the padding and spacing definition from
// NotificationViewBase to this class.

constexpr gfx::Insets kContentRowPadding(0, 12, 16, 12);
// TODO(tetsui): Move |kIconViewSize| to public/cpp/message_center_constants.h
// and merge with contradicting |kNotificationIconSize|.
constexpr gfx::Size kIconViewSize(36, 36);
constexpr gfx::Insets kLeftContentPadding(2, 4, 0, 4);
constexpr gfx::Insets kLeftContentPaddingWithIcon(2, 4, 0, 12);

constexpr int kMessageViewWidthWithIcon =
    kNotificationWidth - kIconViewSize.width() -
    kLeftContentPaddingWithIcon.left() - kLeftContentPaddingWithIcon.right() -
    kContentRowPadding.left() - kContentRowPadding.right();

constexpr int kMessageViewWidth =
    kNotificationWidth - kLeftContentPadding.left() -
    kLeftContentPadding.right() - kContentRowPadding.left() -
    kContentRowPadding.right();

constexpr int kTitleCharacterLimit =
    kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

}  // namespace

NotificationView::NotificationView(
    const message_center::Notification& notification)
    : NotificationViewBase(notification) {
  // Instantiate view instances and define layout and view hierarchy.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  auto header_row = CreateHeaderRow();
  header_row->AddChildView(CreateControlButtonsView());

  auto content_row = CreateContentRow();

  auto left_content = CreateLeftContentView();
  auto* left_content_ptr_ = content_row->AddChildView(std::move(left_content));
  static_cast<views::BoxLayout*>(content_row->GetLayoutManager())
      ->SetFlexForView(left_content_ptr_, 1);
  content_row->AddChildView(CreateRightContentView());

  AddChildView(std::move(header_row));
  AddChildView(std::move(content_row));
  AddChildView(CreateImageContainerView());
  AddChildView(CreateInlineSettingsView());
  AddChildView(CreateActionsRow());

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);
}

NotificationView::~NotificationView() = default;

void NotificationView::CreateOrUpdateTitleView(
    const Notification& notification) {
  if (notification.title().empty() ||
      notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    if (title_view_) {
      DCHECK(left_content()->Contains(title_view_));
      left_content()->RemoveChildViewT(title_view_);
      title_view_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), kTitleCharacterLimit, gfx::WORD_BREAK);
  if (!title_view_) {
    title_view_ = AddViewToLeftContent(GenerateTitleView(title));
  } else {
    title_view_->SetText(title);
    ReorderViewInLeftContent(title_view_);
  }
}

void NotificationView::UpdateViewForExpandedState(bool expanded) {
  // TODO(tetsui): Workaround https://crbug.com/682266 by explicitly setting
  // the width.
  // Ideally, we should fix the original bug, but it seems there's no obvious
  // solution for the bug according to https://crbug.com/678337#c7, we should
  // ensure that the change won't break any of the users of BoxLayout class.
  const int message_view_width =
      (IsIconViewShown() ? kMessageViewWidthWithIcon : kMessageViewWidth) -
      GetInsets().width();
  if (title_view_)
    title_view_->SizeToFit(message_view_width);
  NotificationViewBase::UpdateViewForExpandedState(expanded);
}

}  // namespace message_center
