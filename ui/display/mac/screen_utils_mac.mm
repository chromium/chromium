// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/screen_utils_mac.h"

namespace display {

NSScreen* GetNSScreenFromDisplayID(CGDirectDisplayID display_id) {
  for (NSScreen* screen in NSScreen.screens) {
    CGDirectDisplayID screen_display_id = kCGNullDirectDisplay;
    if (@available(macOS 26, *)) {
      screen_display_id = screen.CGDirectDisplayID;
    } else {
      screen_display_id =
          [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
    }
    if (screen_display_id == display_id) {
      return screen;
    }
  }

  return nullptr;
}

base::TimeDelta GetNSScreenRefreshInterval(CGDirectDisplayID display_id) {
  NSScreen* screen = GetNSScreenFromDisplayID(display_id);
  base::TimeDelta interval;

  if (screen) {
    interval = base::Seconds(1) * screen.minimumRefreshInterval;
  }

  // For ExternalDisplayLinkMac, we start to use a display link before it's
  // actually created in the browser. If this NSScreen is invalid, just use a
  // default value. Viz will be updated again later if anything goes wrong in
  // the browser.
  if (interval.is_positive()) {
    return interval;
  } else {
    return base::Hertz(60);
  }
}

void GetNSScreenRefreshIntervalRange(CGDirectDisplayID display_id,
                                     base::TimeDelta& min_interval,
                                     base::TimeDelta& max_interval,
                                     base::TimeDelta& granularity) {
  NSScreen* screen = GetNSScreenFromDisplayID(display_id);

  if (screen) {
    min_interval = base::Seconds(1) * screen.minimumRefreshInterval;
    max_interval = base::Seconds(1) * screen.maximumRefreshInterval;
    granularity = base::Seconds(1) * screen.displayUpdateGranularity;
  } else {
    min_interval = max_interval = granularity = base::Seconds(1) / 60.0;
  }
}

base::TimeDelta GetCGRefreshInterval(CGDirectDisplayID display_id) {
  // Query the current display mode to retrieve its refresh rate.
  CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
  if (mode == nullptr) {
    return base::Hertz(60);
  }

  double refresh_rate = CGDisplayModeGetRefreshRate(mode);
  CGDisplayModeRelease(mode);

  // A refresh rate of 0 could indicate that the display does not have a fixed
  // refresh rate (e.g., variable refresh rate displays) or the query failed.
  // Fall back to a default of 60Hz.
  if (refresh_rate == 0.0) {
    return base::Hertz(60);
  }

  return base::Hertz(refresh_rate);
}

}  // namespace display
