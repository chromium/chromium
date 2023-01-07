// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_

#include <memory>

#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {

class MESSAGE_CENTER_EXPORT NotificationControlButtonFactory {
 public:
  NotificationControlButtonFactory() = default;
  NotificationControlButtonFactory(const NotificationControlButtonFactory&) =
      delete;
  NotificationControlButtonFactory& operator=(
      const NotificationControlButtonFactory&) = delete;
  virtual ~NotificationControlButtonFactory() = default;

  // Base function for notification control button i.e. close, settings, snooze
  // buttons, used in the `NotificationControlButtonsView`. Chrome uses
  // a PaddedButton, but ash uses an ash::IconButton.
  virtual std::unique_ptr<views::ImageButton> CreateButton(
      views::Button::PressedCallback callback);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTON_FACTORY_H_
