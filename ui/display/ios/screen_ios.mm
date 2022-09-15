// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace display {
namespace {

class ScreenIos : public ScreenBase {
 public:
  ScreenIos() {
    UIScreen* mainScreen = [UIScreen mainScreen];
    CHECK(mainScreen);
    Display display(0, gfx::Rect(mainScreen.bounds));
    display.set_device_scale_factor([mainScreen scale]);
    ProcessDisplayChanged(display, true /* is_primary */);
  }

  ScreenIos(const ScreenIos&) = delete;
  ScreenIos& operator=(const ScreenIos&) = delete;

  gfx::Point GetCursorScreenPoint() override {
    NOTIMPLEMENTED();
    return gfx::Point(0, 0);
  }

  bool IsWindowUnderCursor(gfx::NativeWindow window) override {
    NOTIMPLEMENTED();
    return false;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return gfx::NativeWindow();
  }

  int GetNumDisplays() const override {
#if TARGET_IPHONE_SIMULATOR
    // UIScreen does not reliably return correct results on the simulator.
    return 1;
#else
    return [[UIScreen screens] count];
#endif
  }
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  return [view window];
}

Screen* CreateNativeScreen() {
  return new ScreenIos;
}

}  // namespace gfx
