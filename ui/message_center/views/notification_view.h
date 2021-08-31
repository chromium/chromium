// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/notification_view_base.h"

namespace message_center {

// Customized NotificationViewBase for notification on all platforms other
// than ChromeOS. This view is used to displays all current types of
// notification (web, basic, image, and list) except custom notification.
class MESSAGE_CENTER_EXPORT NotificationView : public NotificationViewBase {
 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit NotificationView(const message_center::Notification& notification);
  NotificationView(const NotificationView&) = delete;
  NotificationView& operator=(const NotificationView&) = delete;
  ~NotificationView() override;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_