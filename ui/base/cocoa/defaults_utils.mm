// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/defaults_utils.h"

#include <AppKit/AppKit.h>

#include <optional>

#include "base/time/time.h"

namespace {

bool& BlinkPeriodNeedsRefresh() {
  static bool blink_period_needs_refresh = []() {
    [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationWillBecomeActiveNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  // Refresh the insertion point blink period in case the user
                  // changed it in System Preferences. Call the original
                  // function to set the flag. The compiler doesn't like
                  // blink_period_needs_refresh's static scope so it won't let
                  // us pass it into the block. There's no worry of infinite
                  // recursion as well never hit this code path a second time.
                  BlinkPeriodNeedsRefresh() = true;
                }];
    return true;
  }();

  return blink_period_needs_refresh;
}

}  // namespace

namespace ui {

std::optional<base::TimeDelta> TextInsertionCaretBlinkPeriodFromDefaults() {
  static std::optional<base::TimeDelta> blink_period;

  if (!BlinkPeriodNeedsRefresh()) {
    return blink_period;
  }

  BlinkPeriodNeedsRefresh() = false;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // See AppKit's `-[NSTextView _restartBlinkTimer]`.
  NSInteger on_period_ms =
      [defaults integerForKey:@"NSTextInsertionPointBlinkPeriodOn"];
  NSInteger off_period_ms =
      [defaults integerForKey:@"NSTextInsertionPointBlinkPeriodOff"];

  // If any of the periods is negative the requested blink time makes no sense.
  // In that case use the default blink time.
  if ((on_period_ms == 0 && off_period_ms == 0) || on_period_ms < 0 ||
      off_period_ms < 0) {
    blink_period = std::nullopt;
    return blink_period;
  }

  const int kMaximumReasonableIntervalMs =
      60 * base::Time::kMillisecondsPerSecond;
  if (on_period_ms > kMaximumReasonableIntervalMs) {
    // Treat an exceedingly long `on_period_ms` as permanently enabling
    // the cursor. In Blink/Views this is signaled by a blink period of 0.
    // Conversely, a high `off_period_ms` would permanently disable the cursor,
    // but Blink and Views don't support that so it's not implemented here.
    blink_period = base::TimeDelta();
  } else if (on_period_ms || off_period_ms) {
    // When both on and off periods are defined take the average (neither Blink
    // nor Views support separate on and off intervals).
    blink_period = base::Milliseconds((on_period_ms + off_period_ms) / 2);
  }

  return blink_period;
}

bool& BlinkPeriodRefreshFlagForTesting() {
  return BlinkPeriodNeedsRefresh();
}

}  // namespace ui
