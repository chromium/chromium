// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/AppKit.h>

#include "ui/base/cocoa/defaults_utils.h"

namespace ui {

bool TextInsertionCaretBlinkPeriod(base::TimeDelta* delta) {
  const int kMaximumReasonableIntervalMs = 60 * 1000;
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  // 10.10+
  double on_period_ms = [defaults
      doubleForKey:@"NSTextInsertionPointBlinkPeriodOn"];
  double off_period_ms = [defaults
      doubleForKey:@"NSTextInsertionPointBlinkPeriodOff"];
  // 10.9
  double period_ms = [defaults
      doubleForKey:@"NSTextInsertionPointBlinkPeriod"];
  if (on_period_ms == 0.0 && off_period_ms == 0.0 && period_ms == 0.0)
    return false;
  // Neither Blink nor Views support having separate on and off intervals, so
  // this function takes the average. There's a special case: setting
  // on_period_ms very high functions to permanently enable the cursor, which is
  // what happens when the blink period in Blink/Views is set to 0. Setting
  // off_period_ms very high would disable the cursor entirely, but Blink/Views
  // do not support that so it's not implemented here.
  if (on_period_ms > kMaximumReasonableIntervalMs ||
      period_ms > kMaximumReasonableIntervalMs) {
    *delta = base::TimeDelta();
  } else if (on_period_ms || off_period_ms) {
    *delta = base::Milliseconds((on_period_ms + off_period_ms) / 2);
  } else {
    *delta = base::Milliseconds(period_ms);
  }
  return true;
}

}  // namespace ui
