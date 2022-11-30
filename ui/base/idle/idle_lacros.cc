// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include <algorithm>

#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/system_idle_cache.h"

namespace ui {

int CalculateIdleTime() {
  base::TimeDelta idle_time =
      base::TimeTicks::Now() -
      chromeos::LacrosService::Get()->system_idle_cache()->last_activity_time();
  // Clamp to positive in case of timing glitch.
  return std::max(0, static_cast<int>(idle_time.InSeconds()));
}

bool CheckIdleStateIsLocked() {
  return chromeos::LacrosService::Get()->system_idle_cache()->is_locked();
}

}  // namespace ui
