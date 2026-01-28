// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win_dcomp.h"

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/direct_composition_support.h"

namespace gl {

constexpr base::TimeDelta kMinimumFallbackTimeout = base::Milliseconds(100);

VSyncThreadWinDComp::VSyncThreadWinDComp()
    : VSyncThreadWin(), vsync_provider_() {}

VSyncThreadWinDComp::~VSyncThreadWinDComp() = default;

base::TimeDelta VSyncThreadWinDComp::GetVsyncInterval() {
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;

  // Use the compositor clock to determine vsync interval, disabled by default
  const bool get_vsync_succeeded =
      vsync_provider_.GetVSyncParametersIfAvailable(&vsync_timebase,
                                                    &vsync_interval);
  DCHECK(get_vsync_succeeded);

  return vsync_interval;
}

gfx::VSyncProvider* VSyncThreadWinDComp::vsync_provider() {
  return &vsync_provider_;
}

bool VSyncThreadWinDComp::WaitForVSyncImpl(base::TimeDelta* vsync_interval) {
  // DCompositionWaitForCompositorClock WILL hang if the adapter becomes
  // disconnected. No monitor should be slower than 100ms. The API will return
  // WAIT_TIMEOUT in the timeout case. When the new adapter comes online, new
  // calls to DCompositionWaitForCompositorClock will latch to the new adapter
  // and go back to tracking VBlank.   DCompositionWaitForCompositorClock
  // returns an error on desktop occlusion and returns early.
  DWORD wait_result = gl::DCompositionWaitForCompositorClock(
      0, nullptr, kMinimumFallbackTimeout.InMilliseconds());

  if (wait_result != WAIT_OBJECT_0) {
    TRACE_EVENT("gpu", "WaitForVSyncImpl", "wait result", wait_result);
    *vsync_interval = kMinimumFallbackTimeout;
    return false;
  }

  *vsync_interval = GetVsyncInterval();

  return true;
}

}  // namespace gl
