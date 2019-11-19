// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "ui/base/idle/idle_internal.h"

namespace ui {

IdleState CalculateIdleState(int idle_threshold) {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value();

  if (CheckIdleStateIsLocked())
    return IDLE_STATE_LOCKED;

  if (CalculateIdleTime() >= idle_threshold)
    return IDLE_STATE_IDLE;

  return IDLE_STATE_ACTIVE;
}

}  // namespace ui
