// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_metrics.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"

namespace {

// Estimated upper limit of what we should record for cache size.
constexpr int kCacheHistogramMax = 100;

// Used to determine if we are running in the browser process for metrics
// purposes.
bool IsBrowserProcess() {
  // Browser process does not specify a type.
  return base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueASCII("type")
      .empty();
}

}  // namespace

namespace ui {

void RecordColorProviderCacheSize(int cache_size) {
  base::UmaHistogramExactLinear("Views.ColorProviderCacheSize", cache_size,
                                kCacheHistogramMax);
}

void RecordNumColorProvidersInitializedDuringOnNativeThemeUpdated(
    int num_providers) {
  base::UmaHistogramCounts100(
      IsBrowserProcess()
          ? "Views.Browser."
            "NumColorProvidersInitializedDuringOnNativeThemeUpdated"
          : "Views.NonBrowser."
            "NumColorProvidersInitializedDuringOnNativeThemeUpdated",
      num_providers);
}

void RecordTimeSpentInitializingColorProvider(base::TimeDelta duration) {
  base::UmaHistogramTimes(
      IsBrowserProcess()
          ? "Views.Browser.TimeSpentInitializingColorProvider"
          : "Views.NonBrowser.TimeSpentInitializingColorProvider",
      duration);
}

void RecordTimeSpentProcessingOnNativeThemeUpdatedEvent(
    base::TimeDelta duration) {
  base::UmaHistogramTimes(
      IsBrowserProcess()
          ? "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent"
          : "Views.NonBrowser.TimeSpentProcessingOnNativeThemeUpdatedEvent",
      duration);
}

}  // namespace ui
