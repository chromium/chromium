// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_POPUP_TIMER_H_
#define UI_MESSAGE_CENTER_POPUP_TIMER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace message_center {

// A class that manages timeout behavior for notification popups.  One instance
// is created per notification popup.
class PopupTimer {
 public:
  // Host the callback for each timer when its time is up.
  class Delegate {
   public:
    virtual void TimerFinished(const std::string& id) = 0;
  };

  // Accepts a notification ID, time until callback, and a reference to the
  // delegate which will be called back.  The reference is a weak pointer so
  // that timers never cause a callback on a destructed object.
  PopupTimer(const std::string& id,
             base::TimeDelta timeout,
             base::WeakPtr<Delegate> delegate);

  PopupTimer(const PopupTimer&) = delete;
  PopupTimer& operator=(const PopupTimer&) = delete;

  ~PopupTimer();

  // Starts running the timer.  Barring a Pause or Reset call, the timer will
  // call back to |delegate| after |timeout| seconds.
  void Start();

  // Stops the timer, and retains the amount of time that has passed so that on
  // subsequent calls to Start the timer will continue where it left off.
  void Pause();

  // Returns whether the underlying timer is running or not.
  bool IsRunning() { return timer_->IsRunning(); }

 private:
  // Notification ID for which this timer applies.
  const std::string id_;

  // Total time that should pass while active before calling TimerFinished.
  base::TimeDelta timeout_;

  // If paused, the amount of time that passed before pause.
  base::TimeDelta passed_;

  // The time that the timer was last started.
  base::Time start_time_;

  // Callback recipient.
  base::WeakPtr<Delegate> timer_delegate_;

  // The actual timer.
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_POPUP_TIMER_H_
