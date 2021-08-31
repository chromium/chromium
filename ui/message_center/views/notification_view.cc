// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

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

}  // namespace message_center
