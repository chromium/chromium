// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ui {

int CalculateIdleTime() {
  // Note that the detector can be null in e.g. Ash unit tests, which can cause
  // a crash if a browser or OS subsystem wants to know the idle state.
  //
  // If it is not possible to check the idle state, assume the system isn't
  // idle.
  const auto* const detector = ui::UserActivityDetector::Get();
  if (!detector) {
    return 0;
  }

  const base::TimeDelta idle_time =
      base::TimeTicks::Now() - detector->last_activity_time();
  return static_cast<int>(idle_time.InSeconds());
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  // Note that the client can be null in e.g. Ash unit tests, which can cause a
  // crash if a browser or OS subsystem wants to know if the computer is locked.
  //
  // If it is not possible to check the locked state, assume the system isn't
  // locked.
  return ash::SessionManagerClient::Get() &&
         ash::SessionManagerClient::Get()->IsScreenLocked();
}

}  // namespace ui
