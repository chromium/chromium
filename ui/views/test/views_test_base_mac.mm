// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <Cocoa/Cocoa.h>

namespace views {

int ViewsTestBase::GetSystemReservedHeightAtTopOfScreen() {
  // Includes gap of 1 px b/w menu bar and title bar.
  CGFloat menu_bar_height = NSHeight(NSScreen.mainScreen.frame) -
                            NSScreen.mainScreen.visibleFrame.origin.y -
                            NSHeight(NSScreen.mainScreen.visibleFrame);
  CGFloat title_bar_height =
      NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                       styleMask:NSWindowStyleMaskTitled]);

  return menu_bar_height + title_bar_height;
}

}  // namespace views
