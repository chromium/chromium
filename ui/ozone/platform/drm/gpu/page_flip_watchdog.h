// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_

#include "base/containers/ring_buffer.h"
#include "base/timer/timer.h"

namespace ui {

// The maximum amount of time we will wait for a new modeset attempt before we
// crash the GPU process.
constexpr base::TimeDelta kWaitForModesetTimeout = base::Seconds(15);

// Number of historical page flip statuses to track.
//
// This number was chosen to collect a large enough sample to detect real
// errors, and with some consideration to the fact that pending page flips can
// be buffered upstream of OzoneDRM.
constexpr size_t kPageFlipWatcherHistorySize = 80;

// Number of times the plane assignment can fail and then succeed within
// |kPageFlipWatcherHistorySize| flip attempts. Exceeding this number will crash
// the process immediately.
//
// For instance, if S is a successful assignment and F is a failure,
// S-S-F-S-S-F-S would count as two flakes.
//
// This number was chosen with consideration of a pending page flip buffer
// upstream that might fill with unflippable plane assignments.
constexpr uint32_t kPlaneAssignmentFlakeThreshold = 3;

// Number of failures permitted by the watchdog, within
// |kPageFlipWatcherHistorySize| flip attempts. Exceeding this number will crash
// the process immediately.
//
// For instance, if S is a successful assignment and F is a failure,
// S-F-S-F-F-F-F-S-S would lead to a crash.
//
// This number was chosen with consideration of a pending page flip buffer
// upstream that might fill with unflippable plane assignments.
constexpr uint32_t kPlaneAssignmentMaximumFailures = 20;

// Tracks the failures and successes of interactions with DRM and handles
// unrecoverable errors by crashing the process.
class PageFlipWatchdog {
 public:
  PageFlipWatchdog();

  PageFlipWatchdog(const PageFlipWatchdog&) = delete;
  PageFlipWatchdog& operator=(const PageFlipWatchdog&) = delete;

  ~PageFlipWatchdog();

  // Notify the watchdog that a successful page flip submission has happened.
  // Ratio of successful and unsuccessful flips are used to determine whether to
  // start the crash timer in |MaybeArmForFailedPlaneAssignment|.
  void OnSuccessfulPageFlip();

  // Called when OzoneDRM can't assign planes for a frame. This may start the
  // crash countdown timer, but this failure is usually intermittent, so
  // we will only start the countdown timer when we see a large percentage of
  // failures.
  void CrashOnFailedPlaneAssignment();
  // Called when an actual DRM commit failed. This will start the crash
  // countdown timer.
  void ArmForFailedCommit();
  // This will reset the destruction countdown timer.
  void Disarm();

 private:
  void StartCrashGpuTimer();

  // Used to crash the GPU process if a page flip commit fails and no new
  // modeset attempts come in.
  base::OneShotTimer crash_gpu_timer_;
  base::RingBuffer<bool, kPageFlipWatcherHistorySize> page_flip_status_tracker_;
  int16_t failed_page_flip_counter_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_WATCHDOG_H_
