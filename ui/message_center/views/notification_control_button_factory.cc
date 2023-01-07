// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_button_factory.h"

#include "ui/message_center/views/padded_button.h"

namespace message_center {

std::unique_ptr<views::ImageButton>
NotificationControlButtonFactory::CreateButton(
    views::Button::PressedCallback callback) {
  return std::make_unique<PaddedButton>(std::move(callback));
}

}  // namespace message_center