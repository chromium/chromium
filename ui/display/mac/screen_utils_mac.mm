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
    return base::Seconds(1) / 60.0;
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

}  // namespace display
