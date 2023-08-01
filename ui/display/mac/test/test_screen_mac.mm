// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/test/test_screen_mac.h"

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace test {

TestScreenMac::TestScreenMac(const gfx::Size& size)
    : TestScreen(/*create_display=*/false) {
  NSScreen* screen = [[NSScreen screens] firstObject];
  CGDirectDisplayID display_id =
      [[screen deviceDescription][@"NSScreenNumber"] unsignedIntValue];

  Display display(display_id);
  display.SetScaleAndBounds(
      1.0f, size.IsEmpty() ? kDefaultScreenBounds : gfx::Rect(size));
  ProcessDisplayChanged(display, /* is_primary = */ true);
}

TestScreenMac::~TestScreenMac() = default;

}  // namespace test
}  // namespace display
