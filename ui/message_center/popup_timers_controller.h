// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_POPUP_TIMERS_CONTROLLER_H_
#define UI_MESSAGE_CENTER_POPUP_TIMERS_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/popup_timer.h"

namespace message_center {

// A class that manages all the timers running for individual notification popup
// windows.  It supports weak pointers in order to allow safe callbacks when
// timers expire.
// We can use SupportsWeakPtr<> because PopupTimer does not
// access this class in its destructor so it is safe to invalidate weak pointers
// after we destroy |popup_timers_|
class MESSAGE_CENTER_EXPORT PopupTimersController
    : public base::SupportsWeakPtr<PopupTimersController>,
      public MessageCenterObserver,
      public PopupTimer::Delegate {
 public:
  explicit PopupTimersController(MessageCenter* message_center);
  ~PopupTimersController() override;

  // MessageCenterObserver implementation.
  void OnNotificationDisplayed(const std::string& id,
                               const DisplaySource source) override;
  void OnNotificationUpdated(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;

  // PopupTimer::Delegate implementation
  void TimerFinished(const std::string& id) override;

  // Pauses all running timers.
  void PauseAll();

  // Continues all managed timers.
  void StartAll();

  // Removes all managed timers.
  void CancelAll();

  // Starts a timer (by creating a PopupTimer) for |id|.
  void StartTimer(const std::string& id,
                  const base::TimeDelta& timeout_in_seconds);

  // Removes and cancels a single popup timer, if it exists.
  void CancelTimer(const std::string& id);

  // Set timeout values used to dismiss notifications.
  static void SetNotificationTimeouts(int default_timeout,
                                      int high_priority_timeout);

  base::TimeDelta GetTimeoutForNotification(Notification* notification);

  int GetNotificationTimeoutDefault();

 private:
  // Weak, global.
  MessageCenter* message_center_;

  // The PopupTimerCollection contains all the managed timers by their ID.
  using PopupTimerCollection =
      std::map<std::string, std::unique_ptr<PopupTimer>>;
  PopupTimerCollection popup_timers_;

  DISALLOW_COPY_AND_ASSIGN(PopupTimersController);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_POPUP_TIMERS_CONTROLLER_H_
