// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include "base/bind.h"
#include "base/callback_helpers.h"
#import "base/mac/scoped_nsobject.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "ui/base/cocoa/window_size_constants.h"
#import "ui/base/test/cocoa_helper.h"

namespace remote_cocoa {

namespace {

struct ImmersiveModeControllerTestHandle {
  NSWindow* browser;
  NSWindow* overlay;
};

// Close the returned overlay before end of the test.
ImmersiveModeControllerTestHandle CreateImmersiveModeControllerTestHandle(
    NSWindow* browser) {
  // Make the test window able handle hosting
  // NSTitlebarAccessoryViewControllers.
  browser.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                      NSWindowStyleMaskMiniaturizable |
                      NSWindowStyleMaskResizable;

  // Create a blank overlay window.
  NativeWidgetMacNSWindow* overlay = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [overlay setFrame:NSMakeRect(0, 0, 200, 100) display:YES];
  [browser addChildWindow:overlay ordered:NSWindowAbove];
  EXPECT_EQ(overlay.isVisible, YES);
  return {browser, overlay};
}

}  // namespace

using CocoaImmersiveModeControllerTest = ui::CocoaTest;

// Test ImmersiveModeController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, ImmersiveModeController) {
  ImmersiveModeControllerTestHandle handle =
      CreateImmersiveModeControllerTestHandle(test_window());
  bool view_will_appear_ran = false;
  // Controller under test.
  auto immersive_mode_controller = std::make_unique<ImmersiveModeController>(
      handle.browser, handle.overlay,
      base::BindOnce(
          [](bool* view_will_appear_ran) { *view_will_appear_ran = true; },
          &view_will_appear_ran));
  immersive_mode_controller->Enable();
  EXPECT_TRUE(view_will_appear_ran);
  EXPECT_EQ(handle.browser.titlebarAccessoryViewControllers.count, 1u);

  // Reset immersive_mode_controller before closing the overlay window.
  immersive_mode_controller.reset();
  EXPECT_EQ(handle.browser.titlebarAccessoryViewControllers.count, 0u);

  [handle.overlay close];
}

// Test that child windows in immersive mode properly balance the revealed lock
// count.
TEST_F(CocoaImmersiveModeControllerTest, ChildWindowRevealLock) {
  ImmersiveModeControllerTestHandle handle =
      CreateImmersiveModeControllerTestHandle(test_window());

  // Controller under test.
  auto immersive_mode_controller = std::make_unique<ImmersiveModeController>(
      handle.browser, handle.overlay, base::DoNothing());
  immersive_mode_controller->Enable();

  // Create a popup.
  CocoaTestHelperWindow* popup = [[CocoaTestHelperWindow alloc] init];
  EXPECT_EQ(popup.isVisible, NO);
  EXPECT_EQ(immersive_mode_controller->revealed_lock_count(), 0);

  // Add the popup as a child of overlay.
  [handle.overlay addChildWindow:popup ordered:NSWindowAbove];
  EXPECT_EQ(popup.isVisible, YES);
  EXPECT_EQ(immersive_mode_controller->revealed_lock_count(), 1);

  // Make sure that closing the popup results in the reveal lock count
  // decrementing.
  [popup close];
  EXPECT_EQ(immersive_mode_controller->revealed_lock_count(), 0);

  // Reset immersive_mode_controller before closing the overlay window.
  immersive_mode_controller.reset();
  [handle.overlay close];
}

}  // namespace remote_cocoa
