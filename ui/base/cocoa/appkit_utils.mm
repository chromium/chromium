// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/appkit_utils.h"

#include <cmath>

#include "base/mac/mac_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Double-click in window title bar actions.
enum class DoubleClickAction {
  NONE,
  MINIMIZE,
  MAXIMIZE,
};

// Values of com.apple.trackpad.forceClick corresponding to "Look up & data
// detectors" in System Preferences -> Trackpad -> Point & Click.
enum class ForceTouchAction {
  NONE = 0,        // Unchecked or set to "Tap with three fingers".
  QUICK_LOOK = 1,  // Set to "Force Click with one finger".
};

}  // namespace

namespace ui {

bool ForceClickInvokesQuickLook() {
  return [NSUserDefaults.standardUserDefaults
             integerForKey:@"com.apple.trackpad.forceClick"] ==
         static_cast<NSInteger>(ForceTouchAction::QUICK_LOOK);
}

bool IsCGFloatEqual(CGFloat a, CGFloat b) {
  return std::fabs(a - b) <= std::numeric_limits<CGFloat>::epsilon();
}

}  // namespace ui
