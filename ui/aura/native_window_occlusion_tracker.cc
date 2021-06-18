// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker.h"

#include "build/build_config.h"
#include "ui/aura/window_tree_host.h"

#if defined(OS_WIN)
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif  // OS_WIN

namespace aura {

NativeWindowOcclusionTracker::NativeWindowOcclusionTracker() = default;
NativeWindowOcclusionTracker::~NativeWindowOcclusionTracker() = default;

void NativeWindowOcclusionTracker::EnableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if defined(OS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Enable(
        host->window());
  }
#endif  // defined(OS_WIN)
}

void NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if defined(OS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    host->SetNativeWindowOcclusionState(Window::OcclusionState::UNKNOWN);
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Disable(
        host->window());
  }
#endif  // defined(OS_WIN)
}

}  // namespace aura
