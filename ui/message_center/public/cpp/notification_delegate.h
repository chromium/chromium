// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/message_center_public_export.h"

namespace message_center {

// Handles actions performed on a notification.
class MESSAGE_CENTER_PUBLIC_EXPORT NotificationObserver {
 public:
  // Called when the desktop notification is closed. If closed by a user
  // explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user) {}

  // Called when a desktop notification is clicked. |button_index| is filled in
  // if a button was clicked (as opposed to the body of the notification) while
  // |reply| is filled in if there was an input field associated with the
  // button.
  virtual void Click(const absl::optional<int>& button_index,
                     const absl::optional<std::u16string>& reply) {}

  // Called when the user clicks the settings button in a notification which has
  // a DELEGATE settings button action.
  virtual void SettingsClick() {}

  // Called when the user attempts to disable the notification.
  virtual void DisableNotification() {}
};

// Ref counted version of NotificationObserver, required to satisfy
// message_center::Notification::delegate_.
class MESSAGE_CENTER_PUBLIC_EXPORT NotificationDelegate
    : public NotificationObserver,
      public base::RefCountedThreadSafe<NotificationDelegate> {
 protected:
  virtual ~NotificationDelegate() = default;

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

// A pass-through which converts the RefCounted requirement to a WeakPtr
// requirement. This class replaces the need for individual delegates that pass
// through to an actual controller class, and which only exist because the
// actual controller has a strong ownership model.
class MESSAGE_CENTER_PUBLIC_EXPORT ThunkNotificationDelegate
    : public NotificationDelegate {
 public:
  explicit ThunkNotificationDelegate(base::WeakPtr<NotificationObserver> impl);

  ThunkNotificationDelegate(const ThunkNotificationDelegate&) = delete;
  ThunkNotificationDelegate& operator=(const ThunkNotificationDelegate&) =
      delete;

  // NotificationDelegate:
  void Close(bool by_user) override;
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;
  void SettingsClick() override;
  void DisableNotification() override;

 protected:
  ~ThunkNotificationDelegate() override;

 private:
  base::WeakPtr<NotificationObserver> impl_;
};

// A simple notification delegate which invokes the passed closure when the body
// or a button is clicked.
class MESSAGE_CENTER_PUBLIC_EXPORT HandleNotificationClickDelegate
    : public NotificationDelegate {
 public:
  // The parameter is the index of the button that was clicked, or nullopt if
  // the body was clicked.
  using ButtonClickCallback =
      base::RepeatingCallback<void(absl::optional<int>)>;

  // Creates a delegate that handles clicks on a button or on the body.
  explicit HandleNotificationClickDelegate(const ButtonClickCallback& callback);

  // Creates a delegate that only handles clicks on the body of the
  // notification.
  explicit HandleNotificationClickDelegate(
      const base::RepeatingClosure& closure);

  HandleNotificationClickDelegate(const HandleNotificationClickDelegate&) =
      delete;
  HandleNotificationClickDelegate& operator=(
      const HandleNotificationClickDelegate&) = delete;

  // Overrides the callback with one that handles clicks on a button or on the
  // body.
  void SetCallback(const ButtonClickCallback& callback);

  // Overrides the callback with one that only handles clicks on the body of the
  // notification.
  void SetCallback(const base::RepeatingClosure& closure);

  // NotificationDelegate overrides:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

 protected:
  ~HandleNotificationClickDelegate() override;

 private:
  ButtonClickCallback callback_;
};

}  //  namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_
