// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_

#include "base/timer/timer.h"

namespace ui {

// The maximum amount of time we will wait for a new modeset attempt before we
// crash the GPU process.
constexpr base::TimeDelta kWaitForModesetTimeout = base::Seconds(15);

// Tracks the failures and successes of interactions with DRM and handles
// unrecoverble errors by crashing the process.
class PageFlipWatchdog {
 public:
  PageFlipWatchdog();

  PageFlipWatchdog(const PageFlipWatchdog&) = delete;
  PageFlipWatchdog& operator=(const PageFlipWatchdog&) = delete;

  ~PageFlipWatchdog();

  // This will start the crash countdown timer.
  void Arm();
  // This will reset the crash countdown timer.
  void Disarm();

 private:
  // Used to crash the GPU process if a page flip commit fails and no new
  // modeset attempts come in.
  base::OneShotTimer crash_gpu_timer_;
  int16_t failed_page_flip_counter_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_
