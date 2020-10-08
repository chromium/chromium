// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOUCH_BAR_UTIL_H
#define UI_BASE_COCOA_TOUCH_BAR_UTIL_H

#import <Cocoa/Cocoa.h>
#include <os/availability.h>

#include "base/component_export.h"

namespace ui {

// The touch bar actions that are being recorded in a histogram. These values
// should not be re-ordered or removed.
enum TouchBarAction {
  BACK = 0,
  FORWARD,
  STOP,
  RELOAD,
  HOME,
  SEARCH,
  STAR,
  NEW_TAB,
  CREDIT_CARD_AUTOFILL,
  TEXT_SUGGESTION,
  TOUCH_BAR_ACTION_COUNT
};

// Logs the sample's UMA metrics into the DefaultTouchBar.Metrics histogram.
COMPONENT_EXPORT(UI_BASE) void LogTouchBarUMA(TouchBarAction command);

// Creates a touch bar identifier with the given |id|.
COMPONENT_EXPORT(UI_BASE) NSString* GetTouchBarId(NSString* touch_bar_id);

// Creates a touch Bar jtem identifier.
COMPONENT_EXPORT(UI_BASE)
NSString* GetTouchBarItemId(NSString* touch_bar_id, NSString* item_id);

}  // namespace ui

#endif  // UI_BASE_COCOA_TOUCH_BAR_UTIL_H
