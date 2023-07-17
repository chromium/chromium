// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_provider_win.h"

#include <dwmapi.h>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_features.h"

namespace gl {

VSyncProviderWin::VSyncProviderWin(gfx::AcceleratedWidget window)
    : window_(window) {
}

VSyncProviderWin::~VSyncProviderWin() {}

// static
void VSyncProviderWin::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;

  // Prewarm sandbox
  ::LoadLibrary(L"dwmapi.dll");
}

void VSyncProviderWin::GetVSyncParameters(UpdateVSyncCallback callback) {
  base::TimeTicks timebase;
  base::TimeDelta interval;
  if (GetVSyncParametersIfAvailable(&timebase, &interval))
    std::move(callback).Run(timebase, interval);
}

bool VSyncProviderWin::GetVSyncParametersIfAvailable(
    base::TimeTicks* out_timebase,
    base::TimeDelta* out_interval) {
  TRACE_EVENT0("gpu", "WinVSyncProvider::GetVSyncParameters");

  base::TimeTicks timebase;
  base::TimeDelta interval;

  if (base::win::GetVersion() >= base::win::Version::WIN11_22H2 &&
      base::FeatureList::IsEnabled(
          features::kUsePrimaryMonitorVSyncIntervalOnSV3)) {
    // This is a simplified initial approach to fix crbug.com/1456399
    // In Windows SV3 builds DWM will operate with per monitor refresh
    // rates. As a result of this, DwmGetCompositionTimingInfo is no longer
    // guaranteed to align with the primary monitor but will instead align
    // with the current highest refresh rate monitor. This can cause issues
    // in clients which may be waiting on the primary monitor's vblank as
    // the reported interval may no longer match with the vblank wait.
    // To work around this discrepancy get the VSync interval directly from
    // monitor associated with window_ or the primary monitor.
    HMONITOR monitor;
    if (window_) {
      monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
    } else {
      monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFOEX monitor_info;
    monitor_info.cbSize = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(monitor, &monitor_info)) {
      DEVMODE display_info;
      display_info.dmSize = sizeof(DEVMODE);
      display_info.dmDriverExtra = 0;
      if (EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
                              &display_info) &&
          display_info.dmDisplayFrequency > 1) {
        interval = base::Microseconds(
            (1.0 / static_cast<double>(display_info.dmDisplayFrequency)) *
            base::Time::kMicrosecondsPerSecond);
      }
    }
  } else {
    // Query the DWM timing info first if available. This will provide the most
    // precise values.
    DWM_TIMING_INFO timing_info;
    timing_info.cbSize = sizeof(timing_info);
    HRESULT result = DwmGetCompositionTimingInfo(NULL, &timing_info);
    if (result == S_OK) {
      // Calculate an interval value using the rateRefresh numerator and
      // denominator.
      base::TimeDelta rate_interval;
      if (timing_info.rateRefresh.uiDenominator > 0 &&
          timing_info.rateRefresh.uiNumerator > 0) {
        // Swap the numerator/denominator to convert frequency to period.
        rate_interval =
            base::Microseconds(timing_info.rateRefresh.uiDenominator *
                               base::Time::kMicrosecondsPerSecond /
                               timing_info.rateRefresh.uiNumerator);
      }

      if (base::TimeTicks::IsHighResolution()) {
        // qpcRefreshPeriod is very accurate but noisy, and must be used with
        // a high resolution timebase to avoid frequently missing Vsync.
        timebase = base::TimeTicks::FromQPCValue(
            static_cast<LONGLONG>(timing_info.qpcVBlank));
        interval = base::TimeDelta::FromQPCValue(
            static_cast<LONGLONG>(timing_info.qpcRefreshPeriod));
        // Check for interval values that are impossibly low. A 29 microsecond
        // interval was seen (from a qpcRefreshPeriod of 60).
        if (interval < base::Milliseconds(1)) {
          interval = rate_interval;
        }
        // Check for the qpcRefreshPeriod interval being improbably small
        // compared to the rateRefresh calculated interval, as another
        // attempt at detecting driver bugs.
        if (!rate_interval.is_zero() && interval < rate_interval / 2) {
          interval = rate_interval;
        }
      } else {
        // If FrameTime is not high resolution, we do not want to translate
        // the QPC value provided by DWM into the low-resolution timebase,
        // which would be error prone and jittery. As a fallback, we assume
        // the timebase is zero and use rateRefresh, which may be rounded but
        // isn't noisy like qpcRefreshPeriod, instead. The fact that we don't
        // have a timebase here may lead to brief periods of jank when our
        // scheduling becomes offset from the hardware vsync.
        interval = rate_interval;
      }
    } else {
      // When DWM compositing is active all displays are normalized to the
      // refresh rate of the primary display, and won't composite any faster.
      // If DWM compositing is disabled, though, we can use the refresh rates
      // reported by each display, which will help systems that have mis-matched
      // displays that run at different frequencies.
      HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
      MONITORINFOEX monitor_info;
      monitor_info.cbSize = sizeof(MONITORINFOEX);
      if (GetMonitorInfo(monitor, &monitor_info)) {
        DEVMODE display_info;
        display_info.dmSize = sizeof(DEVMODE);
        display_info.dmDriverExtra = 0;
        if (EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
                                &display_info) &&
            display_info.dmDisplayFrequency > 1) {
          interval = base::Microseconds(
              (1.0 / static_cast<double>(display_info.dmDisplayFrequency)) *
              base::Time::kMicrosecondsPerSecond);
        }
      }
    }
  }

  if (interval.is_zero())
    return false;

  *out_timebase = timebase;
  *out_interval = interval;
  return true;
}

bool VSyncProviderWin::SupportGetVSyncParametersIfAvailable() const {
  return true;
}

bool VSyncProviderWin::IsHWClock() const {
  return true;
}

}  // namespace gl
