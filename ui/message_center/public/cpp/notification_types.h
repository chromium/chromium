// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_TYPES_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_TYPES_H_

namespace message_center {

// Notification types. Used to determine the view that will represent a
// notification.
enum NotificationType {
  NOTIFICATION_TYPE_SIMPLE = 0,
  DEPRECATED_NOTIFICATION_TYPE_BASE_FORMAT =
      1,  // Use `NOTIFICATION_TYPE_SIMPLE` instead.
  NOTIFICATION_TYPE_IMAGE = 2,
  NOTIFICATION_TYPE_MULTIPLE = 3,
  NOTIFICATION_TYPE_PROGRESS = 4,  // Notification with progress bar.
  NOTIFICATION_TYPE_CUSTOM = 5,
  NOTIFICATION_TYPE_CONVERSATION = 6,

  // Add new values before this line.
  NOTIFICATION_TYPE_LAST = NOTIFICATION_TYPE_CONVERSATION
};

enum NotificationPriority {
  MIN_PRIORITY = -2,
  LOW_PRIORITY = -1,
  // In ChromeOS, if priority < `DEFAULT_PRIORITY`, the notification will be
  // silently added to the tray (no pop-up will be shown).
  DEFAULT_PRIORITY = 0,
  // Priorities > `DEFAULT_PRIORITY` have the capability to wake the display up
  // if it was off.
  HIGH_PRIORITY = 1,
  MAX_PRIORITY = 2,

  // Top priority for system-level notifications.. This can't be set from
  // `kPriorityKey`, instead you have to call `SetSystemPriority()` of
  // Notification object.
  SYSTEM_PRIORITY = 3,
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_TYPES_H_
