// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_
#define UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_

#include "build/build_config.h"
#include "ui/aura/aura_export.h"

namespace aura {

class WindowTreeHost;

// This class is a shim between WindowOcclusionTracker and os-specific
// window occlusion tracking classes (currently just
// NativeWindowOcclusionTrackerWin).
class AURA_EXPORT NativeWindowOcclusionTracker {
 public:
  NativeWindowOcclusionTracker() = delete;
  NativeWindowOcclusionTracker(const NativeWindowOcclusionTracker&) = delete;
  NativeWindowOcclusionTracker& operator=(const NativeWindowOcclusionTracker&) =
      delete;
  ~NativeWindowOcclusionTracker() = delete;

  // Enables native window occlusion tracking for the native window |host|
  // represents.
  static void EnableNativeWindowOcclusionTracking(WindowTreeHost* host);

  // Disables native window occlusion tracking for the native window |host|
  // represents.
  static void DisableNativeWindowOcclusionTracking(WindowTreeHost* host);

  // Returns whether native window occlusion tracking is always enabled.
  static bool IsNativeWindowOcclusionTrackingAlwaysEnabled(
      WindowTreeHost* host);

 private:
  friend class WindowTreeHostWithReleaseTest;
  friend class WindowTreeHostWithThrottleTest;
  friend class WindowTreeHostWithThrottleAndReleaseTest;
};

}  // namespace aura

#endif  // UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_
