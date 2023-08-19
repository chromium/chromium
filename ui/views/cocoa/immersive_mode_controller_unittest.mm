// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller.h"
#include "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "ui/base/cocoa/window_size_constants.h"
#import "ui/base/test/cocoa_helper.h"

namespace {
const double kBrowserHeight = 200;
const double kBrowserWidth = 400;
const double kOverlayViewHeight = 100;
const double kOverlayViewWidth = kBrowserWidth;
const double kTabOverlayViewHeight = 50;
const double kTabOverlayViewWidth = kBrowserWidth;
const double kTitlebarHeight = 28;
}

namespace remote_cocoa {

class CocoaImmersiveModeControllerTest : public ui::CocoaTest {
 public:
  CocoaImmersiveModeControllerTest() = default;

  CocoaImmersiveModeControllerTest(const CocoaImmersiveModeControllerTest&) =
      delete;
  CocoaImmersiveModeControllerTest& operator=(
      const CocoaImmersiveModeControllerTest&) = delete;

  void SetUp() override {
    ui::CocoaTest::SetUp();

    // Create a blank browser window.
    browser_ =
        [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable |
                                              NSWindowStyleMaskResizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    browser_.releasedWhenClosed = NO;
    [browser_ setFrame:NSMakeRect(0, 0, kBrowserWidth, kBrowserHeight)
               display:YES];
    [browser_ orderBack:nil];

    // Create a blank overlay window.
    overlay_ = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    overlay_.releasedWhenClosed = NO;
    [overlay_ setFrame:NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight)
               display:YES];
    overlay_.contentView =
        [[BridgedContentView alloc] initWithBridge:nullptr bounds:gfx::Rect()];
    [overlay_.contentView
        setFrame:NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight)];
    [browser_ addChildWindow:overlay_ ordered:NSWindowAbove];
    EXPECT_EQ(overlay_.isVisible, YES);

    // Create a blank tab overlay window as a child of overlay window.
    tab_overlay_ = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    tab_overlay_.releasedWhenClosed = NO;
    [tab_overlay_
        setFrame:NSMakeRect(0, 0, kTabOverlayViewWidth, kTabOverlayViewHeight)
         display:YES];
    tab_overlay_.contentView =
        [[BridgedContentView alloc] initWithBridge:nullptr bounds:gfx::Rect()];
    [tab_overlay_.contentView
        setFrame:NSMakeRect(0, 0, kTabOverlayViewWidth, kTabOverlayViewHeight)];
    [overlay_ addChildWindow:tab_overlay_ ordered:NSWindowAbove];
    EXPECT_EQ(tab_overlay_.isVisible, YES);
  }

  void TearDown() override {
    EXPECT_EQ(browser_.titlebarAccessoryViewControllers.count, 0u);

    [tab_overlay_ close];
    tab_overlay_ = nil;
    [overlay_ close];
    overlay_ = nil;
    [browser_ close];
    browser_ = nil;

    ui::CocoaTest::TearDown();
  }

  NSWindow* browser() { return browser_; }
  NSWindow* overlay() { return overlay_; }
  NSWindow* tab_overlay() { return tab_overlay_; }

 private:
  NSWindow* __strong browser_;
  NSWindow* __strong overlay_;
  NSWindow* __strong tab_overlay_;
};

// Test ImmersiveModeController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, ImmersiveModeController) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeController>(browser(), overlay());
  immersive_mode_controller->Enable();
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 1u);
}

// Test that reveal locks work as expected.
TEST_F(CocoaImmersiveModeControllerTest, RevealLock) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeController>(browser(), overlay());
  immersive_mode_controller->Enable();

  // Autohide top chrome.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  // Hidden height is > 0 as a workaround for https://crbug.com/1369643
  EXPECT_LT(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      1);

  // Grab 3 reveal locks and make sure that top chrome is displayed.
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 0);
  immersive_mode_controller->RevealLock();
  immersive_mode_controller->RevealLock();
  immersive_mode_controller->RevealLock();
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 3);
  EXPECT_EQ(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      browser()
          .titlebarAccessoryViewControllers.firstObject.view.frame.size.height);

  // Let go of 2 reveal locks and make sure that top chrome is still displayed.
  immersive_mode_controller->RevealUnlock();
  immersive_mode_controller->RevealUnlock();
  EXPECT_EQ(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      browser()
          .titlebarAccessoryViewControllers.firstObject.view.frame.size.height);

  // Let go of the final reveal lock and make sure top chrome is hidden.
  immersive_mode_controller->RevealUnlock();
  EXPECT_LT(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      1);
}

// Test ImmersiveModeController titlebar frame KVO.
TEST_F(CocoaImmersiveModeControllerTest, TitlebarObserver) {
  // Create a fake NSToolbarFullScreenWindow and associated views.
  NSView* titlebar_container_view = [[NSView alloc]
      initWithFrame:NSMakeRect(0, kOverlayViewHeight, kOverlayViewWidth,
                               kOverlayViewHeight)];

  NSWindow* fullscreen_window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, kOverlayViewWidth, kBrowserHeight)
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  fullscreen_window.releasedWhenClosed = NO;
  [fullscreen_window.contentView addSubview:titlebar_container_view];
  [fullscreen_window orderBack:nil];

  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeController>(browser(), overlay());
  base::WeakPtrFactory<ImmersiveModeController> weak_ptr_factory(
      immersive_mode_controller.get());

  // Grab the content view from the controller and add it to the test
  // `titlebar_container_view`.
  BridgedContentView* overlay_view =
      immersive_mode_controller->overlay_content_view();
  [titlebar_container_view addSubview:overlay_view];
  overlay_view.frame = NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight);

  // Create a titlebar observer. This is the class under test.
  ImmersiveModeTitlebarObserver* titlebar_observer =
      [[ImmersiveModeTitlebarObserver alloc]
             initWithController:weak_ptr_factory.GetWeakPtr()
          titlebarContainerView:titlebar_container_view];

  // Observer the fake titlebar container view.
  [titlebar_container_view addObserver:titlebar_observer
                            forKeyPath:@"frame"
                               options:NSKeyValueObservingOptionInitial |
                                       NSKeyValueObservingOptionNew
                               context:nullptr];

  // Make sure that the overlay view moves along the y axis as the titlebar
  // container view moves. This simulates the titlebar reveal when top chrome is
  // always visible. Down.
  for (int i = 0; i < kTitlebarHeight + 1; ++i) {
    [titlebar_container_view
        setFrame:NSMakeRect(0, kOverlayViewHeight - i, kOverlayViewWidth,
                            kOverlayViewHeight)];
    EXPECT_EQ(overlay().frame.origin.y, 100 - i);
  }

  // And back up.
  for (int i = 1; i <= kTitlebarHeight; ++i) {
    [titlebar_container_view
        setFrame:NSMakeRect(0, (kOverlayViewHeight - kTitlebarHeight) + i,
                            kOverlayViewWidth, kOverlayViewHeight)];
    EXPECT_EQ(overlay().frame.origin.y,
              (kOverlayViewHeight - kTitlebarHeight) + i);
  }

  // Clip the overlay view and make sure the overlay window moves off screen.
  // This simulates top chrome auto hiding.
  if (@available(macOS 11.0, *)) {
    [titlebar_container_view
        setFrame:NSMakeRect(0, kOverlayViewHeight + 1, kOverlayViewWidth,
                            kOverlayViewHeight)];
    if (@available(macOS 12.0, *)) {
      EXPECT_EQ(overlay().frame.origin.y,
                browser().screen.frame.size.height +
                    browser().screen.safeAreaInsets.top);
    } else {
      EXPECT_EQ(overlay().frame.origin.y, browser().screen.frame.size.height);
    }

    // Remove the clip and make sure the overlay window moves back.
    [titlebar_container_view
        setFrame:NSMakeRect(0, kOverlayViewHeight, kOverlayViewWidth,
                            kOverlayViewHeight)];
    EXPECT_EQ(overlay().frame.origin.y, kOverlayViewHeight);
  }

  [titlebar_container_view removeObserver:titlebar_observer
                               forKeyPath:@"frame"];

  [fullscreen_window close];
  fullscreen_window = nil;
}

// Test ImmersiveModeController toolbar visibility.
TEST_F(CocoaImmersiveModeControllerTest, ToolbarVisibility) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(browser(), overlay(),
                                                      tab_overlay());
  immersive_mode_controller->Enable();

  // NSWindowStyleMaskFullSizeContentView is set until the fullscreen transition
  // is complete.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAlways);
  EXPECT_TRUE(browser().styleMask & NSWindowStyleMaskFullSizeContentView);
  immersive_mode_controller->FullscreenTransitionCompleted();
  EXPECT_FALSE(browser().styleMask & NSWindowStyleMaskFullSizeContentView);

  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_TRUE(browser().titlebarAccessoryViewControllers.firstObject.hidden);

  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  EXPECT_FALSE(browser().titlebarAccessoryViewControllers.firstObject.hidden);
}

// Test ImmersiveModeTabbedController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, Tabbed) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(browser(), overlay(),
                                                      tab_overlay());
  immersive_mode_controller->Enable();

  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 2u);
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 1u);
}

// Test ImmersiveModeTabbedController reveal lock tests.
TEST_F(CocoaImmersiveModeControllerTest, TabbedRevealLock) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(browser(), overlay(),
                                                      tab_overlay());
  immersive_mode_controller->Enable();
  immersive_mode_controller->FullscreenTransitionCompleted();

  // Autohide top chrome.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);

  // A visible NSToolbar will reveal the titlebar, which hosts the tab view
  // controller. Make sure reveal lock and unlock work as expected.
  EXPECT_FALSE(browser().toolbar.visible);
  immersive_mode_controller->RevealLock();
  EXPECT_TRUE(browser().toolbar.visible);
  immersive_mode_controller->RevealUnlock();
  EXPECT_FALSE(browser().toolbar.visible);

  // Make sure the visibility state doesn't change while a reveal lock is
  // active.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAlways);
  EXPECT_TRUE(browser().toolbar.visible);
  immersive_mode_controller->RevealLock();
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  EXPECT_TRUE(browser().toolbar.visible);

  // Make sure the visibility state updates after the last reveal lock has
  // been released.
  immersive_mode_controller->RevealUnlock();
  EXPECT_FALSE(browser().toolbar.visible);
}

// Test ImmersiveModeTabbedController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, TabbedChildWindow) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(browser(), overlay(),
                                                      tab_overlay());
  immersive_mode_controller->Enable();
  immersive_mode_controller->FullscreenTransitionCompleted();

  // Autohide top chrome.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);

  // Create a popup.
  CocoaTestHelperWindow* popup = [[CocoaTestHelperWindow alloc] init];
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 0);

  // Add the popup as a child of tab_overlay.
  [tab_overlay() addChildWindow:popup ordered:NSWindowAbove];
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 1);

  // Make sure that closing the popup results in the reveal lock count
  // decrementing.
  [popup close];
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 0);
}

// Test ImmersiveModeTabbedController z-order test.
TEST_F(CocoaImmersiveModeControllerTest, TabbedChildWindowZOrder) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(browser(), overlay(),
                                                      tab_overlay());
  immersive_mode_controller->Enable();
  immersive_mode_controller->FullscreenTransitionCompleted();

  // Create a popup.
  CocoaTestHelperWindow* popup = [[CocoaTestHelperWindow alloc] init];
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 0);

  // Add the popup as a child of overlay.
  [overlay() addChildWindow:popup ordered:NSWindowAbove];

  // Make sure the tab overlay window stays on z-order top.
  EXPECT_EQ(overlay().childWindows.lastObject, tab_overlay());

  [popup close];
}

}  // namespace remote_cocoa
