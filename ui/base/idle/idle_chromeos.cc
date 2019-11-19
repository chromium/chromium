// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/time/time.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ui {

int CalculateIdleTime() {
  base::TimeDelta idle_time = base::TimeTicks::Now() -
      ui::UserActivityDetector::Get()->last_activity_time();
  return static_cast<int>(idle_time.InSeconds());
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  return chromeos::SessionManagerClient::Get()->IsScreenLocked();
}

}  // namespace ui
