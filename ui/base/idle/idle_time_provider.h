// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_IDLE_TIME_PROVIDER_H_
#define UI_BASE_IDLE_IDLE_TIME_PROVIDER_H_

#include "base/time/time.h"

namespace ui {

// Provides an interface for querying a user's idle time and screen state.
class IdleTimeProvider {
 public:
  virtual ~IdleTimeProvider() = default;

  // See ui/base/idle/idle.h for the semantics of these methods.
  virtual base::TimeDelta CalculateIdleTime() = 0;
  virtual bool CheckIdleStateIsLocked() = 0;
};

}  // namespace ui

#endif  // UI_BASE_IDLE_IDLE_TIME_PROVIDER_H_
