// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_USER_ACTIVITY_USER_ACTIVITY_DETECTOR_H_
#define UI_BASE_USER_ACTIVITY_USER_ACTIVITY_DETECTOR_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_observer.h"

namespace ui {

class UserActivityObserver;

// Watches for input events and notifies observers that the user is active.
class COMPONENT_EXPORT(UI_BASE) UserActivityDetector
    : public PlatformEventObserver {
 public:
  // Minimum amount of time between notifications to observers.
  static const int kNotifyIntervalMs;

  // Amount of time that mouse events should be ignored after notification
  // is received that displays' power states are being changed.
  static const int kDisplayPowerChangeIgnoreMouseMs;

  UserActivityDetector(const UserActivityDetector&) = delete;
  UserActivityDetector& operator=(const UserActivityDetector&) = delete;

  // Returns the UserActivityDetector instance.
  static UserActivityDetector* Get();

  base::TimeTicks last_activity_time() const { return last_activity_time_; }
  std::string last_activity_name() const { return last_activity_name_; }

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }
  void set_last_activity_time_for_test(base::TimeTicks value) {
    last_activity_time_ = value;
  }

  bool HasObserver(const UserActivityObserver* observer) const;
  void AddObserver(UserActivityObserver* observer);
  void RemoveObserver(UserActivityObserver* observer);

  // Called when displays are about to be turned on or off.
  void OnDisplayPowerChanging();

  // Handles reports of user activity originating from outside of
  // PlatformEventSource (e.g. the window server).
  void HandleExternalUserActivity();

  // PlatformEventObserver:
  void WillProcessEvent(const PlatformEvent& platform_event) override {}
  void DidProcessEvent(const PlatformEvent& platform_event) override;
  void PlatformEventSourceDestroying() override;

  void ResetStateForTesting();
  void InitPlatformEventSourceObservationForTesting();

 private:
  friend class base::NoDestructor<UserActivityDetector>;
  friend class UserActivityDetectorTest;

  UserActivityDetector();
  ~UserActivityDetector() override;

  // Sets up the observation over the PlatformEventSource. The event source
  // must have been constructed before this is called.
  void InitPlatformEventSourceObservation();

  // Returns |now_for_test_| if set or base::TimeTicks::Now() otherwise.
  base::TimeTicks GetCurrentTime() const;

  // Processes the event after it has been converted from a PlatformEvent.
  void ProcessReceivedEvent(const ui::Event* event);

  // Updates |last_activity_time_|.  Additionally notifies observers and
  // updates |last_observer_notification_time_| if enough time has passed
  // since the last notification.
  void HandleActivity(const ui::Event* event);

  base::ObserverList<UserActivityObserver>::Unchecked observers_;

  // Last time at which user activity was observed.
  base::TimeTicks last_activity_time_;

  // Name of the last user activity event.
  std::string last_activity_name_;

  // Last time at which we notified observers that the user was active.
  base::TimeTicks last_observer_notification_time_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  // If set, mouse events will be ignored until this time is reached. This
  // is to avoid reporting mouse events that occur when displays are turned
  // on or off as user activity.
  base::TimeTicks honor_mouse_events_time_;
};

}  // namespace ui

#endif  // UI_BASE_USER_ACTIVITY_USER_ACTIVITY_DETECTOR_H_
