// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win_dcomp.h"

#include "base/logging.h"
#include "base/trace_event/typed_macros.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_features.h"

namespace gl {

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
  *vsync_interval = GetVsyncInterval();

  // Using INFINITE timeout as it is expected for the wait to return in a
  // timely manner - either at the next vblank or immediately due to desktop
  // occlusion. This behavior matches the DXGI case.
  // DCompositionWaitForCompositorClock returns an error on desktop occlusion
  // and returns early.
  DWORD wait_result =
      gl::DCompositionWaitForCompositorClock(0, nullptr, INFINITE);

  if (wait_result != WAIT_OBJECT_0) {
    TRACE_EVENT1("gpu", "WaitForVSyncImpl", "wait result", wait_result);
    return false;
  }

  return true;
}

}  // namespace gl
