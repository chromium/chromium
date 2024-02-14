// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace aura {
namespace {
// Whether IsNativeWindowOcclusionTrackingAlwaysEnabled() should check for
// CHROME_HEADLESS.
bool g_headless_check_enabled = true;
}  // namespace

// static
void NativeWindowOcclusionTracker::EnableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if BUILDFLAG(IS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Enable(
        host->window());
  }
#endif  // BUILDFLAG(IS_WIN)
}

// static
void NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(
    WindowTreeHost* host) {
#if BUILDFLAG(IS_WIN)
  if (host->IsNativeWindowOcclusionEnabled()) {
    host->SetNativeWindowOcclusionState(Window::OcclusionState::UNKNOWN, {});
    host->set_on_current_workspace(std::nullopt);
    NativeWindowOcclusionTrackerWin::GetOrCreateInstance()->Disable(
        host->window());
  }
#endif  // BUILDFLAG(IS_WIN)
}

// static
bool NativeWindowOcclusionTracker::IsNativeWindowOcclusionTrackingAlwaysEnabled(
    WindowTreeHost* host) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // chromedriver uses the environment variable CHROME_HEADLESS. In this case
  // it expected that native occlusion is not applied.
  static bool is_headless = getenv("CHROME_HEADLESS") != nullptr;
  if ((is_headless && g_headless_check_enabled) ||
      !host->IsNativeWindowOcclusionEnabled() ||
      !base::FeatureList::IsEnabled(
          features::kApplyNativeOcclusionToCompositor)) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  if (!base::FeatureList::IsEnabled(features::kCalculateNativeWinOcclusion)) {
    return false;
  }
#endif

  const std::string type =
      features::kApplyNativeOcclusionToCompositorType.Get();
  return type == features::kApplyNativeOcclusionToCompositorTypeRelease ||
         type == features::kApplyNativeOcclusionToCompositorTypeThrottle ||
         type ==
             features::kApplyNativeOcclusionToCompositorTypeThrottleAndRelease;
#else
  return false;
#endif
}

// static
void NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(bool enabled) {
  g_headless_check_enabled = enabled;
}

}  // namespace aura
