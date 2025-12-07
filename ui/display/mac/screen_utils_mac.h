// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_SCREEN_UTILS_MAC_H_
#define UI_DISPLAY_MAC_SCREEN_UTILS_MAC_H_

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "base/time/time.h"

namespace display {

// Returns the NSScreen object pointer for the caller to retrieve information
// about a screen.
NSScreen* GetNSScreenFromDisplayID(CGDirectDisplayID display_id);

// Returns the minimum refresh interval of the display.
base::TimeDelta GetNSScreenRefreshInterval(CGDirectDisplayID display_id);

// Returns the range of intervals the display can support.
// All screen refresh rates fall between the values in min_interval and
// max_interval with the granularity between the screen’s supported update
// rates.
void GetNSScreenRefreshIntervalRange(CGDirectDisplayID display_id,
                                     base::TimeDelta& min_interval,
                                     base::TimeDelta& max_interval,
                                     base::TimeDelta& granularity);
}  // namespace display

#endif  // UI_DISPLAY_MAC_SCREEN_UTILS_MAC_H_
