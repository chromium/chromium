// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_BLOCKER_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_BLOCKER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/public/cpp/notification.h"

namespace message_center {
class MessageCenter;

// NotificationBlocker manages the availability of notifications based on the
// current system status. Each NotificationBlocker implementation covers a
// single state such as screen lock or fullscreen.
//
// The current notification state is automatically updated when a blocker is
// initialized or destroyed. For instance, the `WelcomeTourNotificationBlocker`
// blocks all existing notifications when it is initialized (see below for how
// to properly initialize a `NotificationBlocker` subclass), and unblocks
// notifications when it is destructed.
//
// An object of a class that derives from this is not ready until `Init()` has
// been called. The following example demonstrates how to properly initialize
// such an object:
//
//   class MyNotificationBlocker : public NotificationBlocker {
//    public:
//     MyNotificationBlocker(MessageCenter* message_center, ...)
//         : NotificationBlocker(message_center), ... { ... }
//    ...
//   };
//
//   void foo() {
//     MessageCenter* message_center = ...;
//     std::unique_ptr<MyNotificationBlocker> my_blocker =
//         std::make_unique<MyNotificationBlocker>(message_center, ...);
//     my_blocker->Init();  /* DON'T FORGET THIS LINE! */
//     ...
//   }
//
// TODO(b/290637034): Improve upon the `Init()` pattern as it is easy to forget
// to call `Init()` when initializing a `NotificationBlocker` subclass.
class MESSAGE_CENTER_EXPORT NotificationBlocker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnBlockingStateChanged(NotificationBlocker* blocker) = 0;
  };

  explicit NotificationBlocker(MessageCenter* message_center);
  virtual ~NotificationBlocker();

  // Registers this `NotificationBlocker` with the `MessageCenter`. This cannot
  // be done in the ctor because registering a notification blocker with the
  // message center may indirectly call the virtual functions below (e.g.
  // `ShouldShowNotification()` and `ShouldShowNotificationAsPopup()`), and
  // virtual functions should not be called from a ctor.
  void Init();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Checks the current state and updates the availability.
  virtual void CheckState() {}

  // Returns true should be shown in the message center. Default returns true
  // always.
  virtual bool ShouldShowNotification(
      const Notification& notification) const;

  // Returns true if this notification should be shown as popups on screen.
  // If it's false, those notifications should be queued.
  // When a blocker starts returning false for a notification which is already
  // shown as a popup, the notification should be closed as a popup immediately.
  virtual bool ShouldShowNotificationAsPopup(
      const Notification& notification) const = 0;

 protected:
  MessageCenter* message_center() { return message_center_; }
  void NotifyBlockingStateChanged();

 private:
  bool is_initialized_ = false;
  base::ObserverList<Observer> observers_;
  raw_ptr<MessageCenter> message_center_;  // weak
};

typedef std::vector<raw_ptr<NotificationBlocker, VectorExperimental>>
    NotificationBlockers;

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_BLOCKER_H_
