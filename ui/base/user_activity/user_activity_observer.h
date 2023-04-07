// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_
#define UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_

#include "base/component_export.h"

namespace ui {
class Event;
}

namespace ui {

// Interface for classes that want to be notified about user activity.
// Implementations should register themselves with UserActivityDetector.
class COMPONENT_EXPORT(UI_BASE) UserActivityObserver {
 public:
  UserActivityObserver(const UserActivityObserver&) = delete;
  UserActivityObserver& operator=(const UserActivityObserver&) = delete;

  // Invoked periodically while the user is active (i.e. generating input
  // events). |event| is the event that triggered the notification; it may
  // be NULL in some cases (e.g. testing, synthetic invocations or external user
  // activities reported by Chrome extensions). The Imprivata extension reports
  // external user activities (with NULL events) via the chrome.login API.
  virtual void OnUserActivity(const ui::Event* event) = 0;

 protected:
  UserActivityObserver() {}
  virtual ~UserActivityObserver() {}
};

}  // namespace ui

#endif  // UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_
