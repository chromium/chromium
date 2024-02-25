// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_provider_win.h"

#include <dwmapi.h>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/win/display_config_helper.h"
#include "ui/gfx/native_widget_types.h"

namespace gl {

namespace {

// Returns a timebase (TimeTicks) & interval (TimeDelta) pair representing last
// vBlank timing and display period (in microseconds) calculated using
// DwmGetCompositionTimingInfo. In cases of failure returns an interval of 0,
// timebase may be 0 in cases where High Resolution Timers are not in use even
// when interval is successfully set.
std::pair<base::TimeTicks, base::TimeDelta> TryGetVSyncParamsFromDwmCompInfo() {
  base::TimeTicks timebase;
  base::TimeDelta interval;

  // Query the DWM timing info first if available. This will provide the most
  // precise values.
  DWM_TIMING_INFO timing_info;
  timing_info.cbSize = sizeof(timing_info);
  HRESULT result = ::DwmGetCompositionTimingInfo(NULL, &timing_info);
  // DwmGetCompositionTimingInfo returns qpcVBlank & qpcRefreshPeriod as type
  // QPC_TIME, which is defined as ULONGLONG. In Chromium time, such as
  // base::TimeDelta, is stored as type LONGLONG. In normal operating conditions
  // we don't expect DwmGetCompositionTimingInfo to return values larger than
  // LLONG_MAX because it is built upon Windows APIs which also treat time as
  // type LONGLONG and on a typical system where the QPC interval is 100ns then
  // a qpcVBlank time of LLONG_MAX would represent ~29K years. There are cases
  // where we can encounter values greater than LLONG_MAX however (see
  // https://crbug.com/1499654), so we want to protect against this by falling
  // back to another interval querying method.
  if (result == S_OK && timing_info.qpcVBlank <= LLONG_MAX &&
      timing_info.qpcRefreshPeriod <= LLONG_MAX) {
    // Calculate an interval value using the rateRefresh numerator and
    // denominator.
    base::TimeDelta rate_interval;
    if (timing_info.rateRefresh.uiDenominator > 0 &&
        timing_info.rateRefresh.uiNumerator > 0) {
      // Swap the numerator/denominator to convert frequency to period.
      rate_interval = base::Microseconds(timing_info.rateRefresh.uiDenominator *
                                         base::Time::kMicrosecondsPerSecond /
                                         timing_info.rateRefresh.uiNumerator);
    }

    if (base::TimeTicks::IsHighResolution()) {
      // qpcRefreshPeriod is very accurate but noisy, and must be used with
      // a high resolution timebase to avoid frequently missing Vsync.
      timebase = base::TimeTicks::FromQPCValue(
          base::checked_cast<LONGLONG>(timing_info.qpcVBlank));
      interval = base::TimeDelta::FromQPCValue(
          base::checked_cast<LONGLONG>(timing_info.qpcRefreshPeriod));
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
  }

  return {timebase, interval};
}

// Returns a TimeDelta representing display period (in microseconds) calculated
// using QueryDisplayConfig. In cases of failure returns a TimeDelta of 0.
base::TimeDelta TryGetVsyncIntervalFromDisplayConfig(
    gfx::AcceleratedWidget window) {
  base::TimeDelta interval;

  HMONITOR monitor =
      window ? ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)
             : ::MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

  if (auto path_info = display::win::GetDisplayConfigPathInfo(monitor);
      path_info) {
    auto& refresh_rate = path_info->targetInfo.refreshRate;
    if (refresh_rate.Denominator != 0 && refresh_rate.Numerator != 0) {
      double micro_seconds = base::ClampDiv(
          base::ClampMul(base::Time::kMicrosecondsPerSecond,
                         static_cast<double>(refresh_rate.Denominator)),
          static_cast<double>(refresh_rate.Numerator));
      interval = base::Microseconds(base::ClampRound<int64_t>(micro_seconds));
    }
  }

  return interval;
}

// Returns a TimeDelta representing display period (in microseconds) calculated
// using EnumDisplaySettings. In cases of failure returns a TimeDelta of 0.
base::TimeDelta TryGetVSyncIntervalFromDisplaySettings(
    gfx::AcceleratedWidget window) {
  base::TimeDelta interval;
  // When DWM compositing is active all displays are normalized to the
  // refresh rate of the primary display, and won't composite any faster.
  // If DWM compositing is disabled, though, we can use the refresh rates
  // reported by each display, which will help systems that have mis-matched
  // displays that run at different frequencies.

  // NOTE: The EnumDisplaySettings API does not support fractional display
  // frequencies, e.g. a display operating at 29.97hz will report a
  // frequency of 29hz.
  HMONITOR monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX monitor_info;
  monitor_info.cbSize = sizeof(MONITORINFOEX);
  if (::GetMonitorInfo(monitor, &monitor_info)) {
    DEVMODE display_info;
    display_info.dmSize = sizeof(DEVMODE);
    display_info.dmDriverExtra = 0;
    if (::EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
                              &display_info) &&
        display_info.dmDisplayFrequency > 1) {
      interval = base::Microseconds(
          (1.0 / base::checked_cast<double>(display_info.dmDisplayFrequency)) *
          base::Time::kMicrosecondsPerSecond);
    }
  }

  return interval;
}

}  // namespace

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

  // Prefer getting vsync parameters from DwmCompositionInfo in order to get a
  // timebase.
  auto [timebase, interval] = TryGetVSyncParamsFromDwmCompInfo();

  // If DwmCompositionInfo wasn't available then prefer getting the interval
  // from QueryDisplayConfig as it supports fractional refresh rates.
  if (interval.is_zero()) {
    interval = TryGetVsyncIntervalFromDisplayConfig(window_);
  }

  if (interval.is_zero()) {
    interval = TryGetVSyncIntervalFromDisplaySettings(window_);
  }

  if (interval.is_zero()) {
    return false;
  }

  *out_timebase = timebase;
  *out_interval = interval;
  return true;
}

// On Windows versions greater than WIN11_22H2 DWM will execute at the
// refresh rate of the fastest operating display, where previously it
// would always align with the primary monitor. As a result of this, some
// clients of VSyncProviderWin need a way to get an interval that still
// aligns with the Primary monitor (e.g. VSyncThreadWin uses primary monitor
// vblanks for its timings so needs to report intervals that align with that).
// GetVSyncIntervalIfAvailable prioritizes this case, but still allows for
// fallbacks to use DwmCompositionInfo or EnumDisplaySettings if needed.
bool VSyncProviderWin::GetVSyncIntervalIfAvailable(
    base::TimeDelta* out_interval) {
  TRACE_EVENT0("gpu", "WinVSyncProvider::GetVSyncIntervalIfAvailable");

  // Prefer getting vsync parameters from QueryDisplayConfig in order to
  // align with window_'s or Primary monitor's vblanks.
  base::TimeDelta interval = TryGetVsyncIntervalFromDisplayConfig(window_);

  // If QueryDisplayConfig wasn't available then prefer DwmCompositionInfo
  // as it supports fractional refresh rates.
  if (interval.is_zero()) {
    base::TimeTicks timebase;
    std::tie(timebase, interval) = TryGetVSyncParamsFromDwmCompInfo();
  }

  if (interval.is_zero()) {
    interval = TryGetVSyncIntervalFromDisplaySettings(window_);
  }

  if (interval.is_zero()) {
    return false;
  }

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
