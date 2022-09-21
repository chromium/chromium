// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"
#include <cstdint>

#include "base/containers/ring_buffer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"

namespace ui {

PageFlipWatchdog::PageFlipWatchdog() = default;

PageFlipWatchdog::~PageFlipWatchdog() = default;

void PageFlipWatchdog::OnSuccessfulPageFlip() {
  page_flip_status_tracker_.SaveToBuffer(true);
}

void PageFlipWatchdog::CrashOnFailedPlaneAssignment() {
  page_flip_status_tracker_.SaveToBuffer(false);
  failed_page_flip_counter_++;

  // Wait until the log of recent page flips is full to avoid crashing
  // too early.
  if (page_flip_status_tracker_.CurrentIndex() <
      page_flip_status_tracker_.BufferSize())
    return;

  bool last_page_flip_status = true;
  uint32_t flakes = 0;
  uint32_t failures = 0;
  for (auto iter = page_flip_status_tracker_.Begin(); iter; ++iter) {
    bool page_flip_status = **iter;
    if (page_flip_status && !last_page_flip_status)
      flakes += 1;
    if (!page_flip_status)
      failures += 1;
    last_page_flip_status = page_flip_status;
  }

  if (flakes >= kPlaneAssignmentFlakeThreshold) {
    LOG(FATAL) << "Plane assignment has flaked " << flakes
               << " times, but the threshold is "
               << kPlaneAssignmentFlakeThreshold
               << ". Crashing the GPU process.";
  }

  if (failures >= kPlaneAssignmentMaximumFailures) {
    LOG(FATAL) << "Plane assignment has failed " << failures << "/"
               << page_flip_status_tracker_.BufferSize()
               << " times, but the threshold is "
               << kPlaneAssignmentMaximumFailures
               << ". Crashing the GPU process.";
  }
}

void PageFlipWatchdog::ArmForFailedCommit() {
  page_flip_status_tracker_.SaveToBuffer(false);
  failed_page_flip_counter_++;
  StartCrashGpuTimer();
}

void PageFlipWatchdog::Disarm() {
  failed_page_flip_counter_ = 0;
  page_flip_status_tracker_.Clear();

  if (crash_gpu_timer_.IsRunning()) {
    crash_gpu_timer_.AbandonAndStop();
    SYSLOG(INFO)
        << "Detected a modeset attempt after " << failed_page_flip_counter_
        << " failed page flips. Aborting GPU process self-destruct with "
        << crash_gpu_timer_.desired_run_time() - base::TimeTicks::Now()
        << " to spare.";
  }
}

void PageFlipWatchdog::StartCrashGpuTimer() {
  if (!crash_gpu_timer_.IsRunning()) {
    DCHECK_GE(failed_page_flip_counter_, 1);
    LOG(WARNING) << "Initiating GPU process self-destruct in "
                 << kWaitForModesetTimeout
                 << " unless a modeset attempt is detected.";

    crash_gpu_timer_.Start(
        FROM_HERE, kWaitForModesetTimeout, base::BindOnce([] {
          LOG(FATAL) << "Failed to modeset within " << kWaitForModesetTimeout
                     << " of the first page flip failure. Crashing GPU "
                        "process.";
        }));
  }
}

}  // namespace ui
