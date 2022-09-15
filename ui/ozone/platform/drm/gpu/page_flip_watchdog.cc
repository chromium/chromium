// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"

#include "base/logging.h"
#include "base/syslog_logging.h"

namespace ui {

PageFlipWatchdog::PageFlipWatchdog() = default;

PageFlipWatchdog::~PageFlipWatchdog() = default;

void PageFlipWatchdog::Arm() {
  failed_page_flip_counter_++;
  if (!crash_gpu_timer_.IsRunning()) {
    DCHECK_EQ(1, failed_page_flip_counter_);
    LOG(WARNING) << "Initiating GPU process self-destruct in "
                 << kWaitForModesetTimeout
                 << " unless a modeset attempt is detected.";

    crash_gpu_timer_.Start(
        FROM_HERE, kWaitForModesetTimeout, base::BindOnce([] {
          LOG(FATAL) << "Failed to modeset within " << kWaitForModesetTimeout
                     << " of the first page flip failure. Crashing GPU "
                        "process. Goodbye.";
        }));
  }
}

void PageFlipWatchdog::Disarm() {
  if (crash_gpu_timer_.IsRunning()) {
    crash_gpu_timer_.AbandonAndStop();
    SYSLOG(INFO)
        << "Detected a modeset attempt after " << failed_page_flip_counter_
        << " failed page flips. Aborting GPU process self-destruct with "
        << crash_gpu_timer_.desired_run_time() - base::TimeTicks::Now()
        << " to spare.";
    failed_page_flip_counter_ = 0;
  }
}

}  // namespace ui
