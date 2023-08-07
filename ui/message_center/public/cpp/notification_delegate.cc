// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notification_delegate.h"

#include "base/check.h"
#include "base/functional/bind.h"

namespace message_center {

// ThunkNotificationDelegate:

ThunkNotificationDelegate::ThunkNotificationDelegate(
    base::WeakPtr<NotificationObserver> impl)
    : impl_(impl) {}

void ThunkNotificationDelegate::Close(bool by_user) {
  if (impl_)
    impl_->Close(by_user);
}

void ThunkNotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (impl_)
    impl_->Click(button_index, reply);
}

void ThunkNotificationDelegate::SettingsClick() {
  if (impl_)
    impl_->SettingsClick();
}

void ThunkNotificationDelegate::DisableNotification() {
  if (impl_)
    impl_->DisableNotification();
}

void ThunkNotificationDelegate::ExpandStateChanged() {
  // Not implemented by default.
}

ThunkNotificationDelegate::~ThunkNotificationDelegate() = default;

// HandleNotificationClickDelegate:

HandleNotificationClickDelegate::HandleNotificationClickDelegate(
    const base::RepeatingClosure& callback) {
  SetCallback(callback);
}

HandleNotificationClickDelegate::HandleNotificationClickDelegate(
    const ButtonClickCallback& callback)
    : callback_(callback) {}

void HandleNotificationClickDelegate::SetCallback(
    const ButtonClickCallback& callback) {
  callback_ = callback;
}

void HandleNotificationClickDelegate::SetCallback(
    const base::RepeatingClosure& closure) {
  if (!closure.is_null()) {
    // Create a callback that consumes and ignores the button index parameter,
    // and just runs the provided closure.
    callback_ = base::BindRepeating(
        [](const base::RepeatingClosure& closure,
           absl::optional<int> button_index) {
          DCHECK(!button_index);
          closure.Run();
        },
        closure);
  }
}

HandleNotificationClickDelegate::~HandleNotificationClickDelegate() {}

void HandleNotificationClickDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (!callback_.is_null())
    callback_.Run(button_index);
}

}  // namespace message_center
