// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_IDLE_H_
#define UI_BASE_IDLE_IDLE_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace ui {

enum IdleState {
  IDLE_STATE_ACTIVE = 0,
  IDLE_STATE_IDLE = 1,    // No activity within threshold.
  IDLE_STATE_LOCKED = 2,  // Only available on supported systems.
  IDLE_STATE_UNKNOWN = 3  // Used when waiting for the Idle state or in error
                          // conditions
};

// For MacOSX, InitIdleMonitor needs to be called first to setup the monitor.
#if BUILDFLAG(IS_APPLE)
COMPONENT_EXPORT(UI_BASE_IDLE) void InitIdleMonitor();
#endif

// Calculate the Idle state. |idle_threshold| is the amount of time (in seconds)
// before the user is considered idle.
COMPONENT_EXPORT(UI_BASE_IDLE) IdleState CalculateIdleState(int idle_threshold);

// Calculate Idle time in seconds.
COMPONENT_EXPORT(UI_BASE_IDLE) int CalculateIdleTime();

// Checks synchronously if Idle state is IDLE_STATE_LOCKED.
COMPONENT_EXPORT(UI_BASE_IDLE) bool CheckIdleStateIsLocked();

}  // namespace ui

#endif  // UI_BASE_IDLE_IDLE_H_
