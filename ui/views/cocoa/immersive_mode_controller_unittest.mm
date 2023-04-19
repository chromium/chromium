// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"
#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#import "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
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
    browser_.reset([[NSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [browser_ setFrame:NSMakeRect(0, 0, kBrowserWidth, kBrowserHeight)
               display:YES];
    [browser_ orderBack:nil];
    browser_.get().releasedWhenClosed = NO;

    // Create a blank overlay window.
    overlay_.reset([[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    overlay_.get().releasedWhenClosed = NO;
    [overlay_ setFrame:NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight)
               display:YES];
    overlay_.get().contentView =
        [[[BridgedContentView alloc] initWithBridge:nullptr
                                             bounds:gfx::Rect()] autorelease];
    [overlay_.get().contentView
        setFrame:NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight)];
    [browser_ addChildWindow:overlay_ ordered:NSWindowAbove];
    EXPECT_EQ(overlay_.get().isVisible, YES);

    // Create a blank tab overlay window as a child of overlay window.
    tab_overlay_.reset([[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    tab_overlay_.get().releasedWhenClosed = NO;
    [tab_overlay_
        setFrame:NSMakeRect(0, 0, kTabOverlayViewWidth, kTabOverlayViewHeight)
         display:YES];
    tab_overlay_.get().contentView =
        [[[BridgedContentView alloc] initWithBridge:nullptr
                                             bounds:gfx::Rect()] autorelease];
    [tab_overlay_.get().contentView
        setFrame:NSMakeRect(0, 0, kTabOverlayViewWidth, kTabOverlayViewHeight)];
    [overlay_ addChildWindow:tab_overlay_ ordered:NSWindowAbove];
    EXPECT_EQ(tab_overlay_.get().isVisible, YES);
  }

  void TearDown() override {
    EXPECT_EQ(browser_.get().titlebarAccessoryViewControllers.count, 0u);

    [tab_overlay_ close];
    tab_overlay_.reset();
    [overlay_ close];
    overlay_.reset();
    [browser_ close];
    browser_.reset();

    ui::CocoaTest::TearDown();
  }

  NSWindow* browser() { return browser_; }
  NSWindow* overlay() { return overlay_; }
  NSWindow* tab_overlay() { return tab_overlay_; }

 private:
  base::scoped_nsobject<NSWindow> browser_;
  base::scoped_nsobject<NSWindow> overlay_;
  base::scoped_nsobject<NSWindow> tab_overlay_;
};

// Test ImmersiveModeController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, ImmersiveModeController) {
  bool view_will_appear_ran = false;
  // Controller under test.
  auto immersive_mode_controller = std::make_unique<ImmersiveModeController>(
      browser(), overlay(),
      base::BindOnce(
          [](bool* view_will_appear_ran) { *view_will_appear_ran = true; },
          &view_will_appear_ran));
  immersive_mode_controller->Enable();
  EXPECT_TRUE(view_will_appear_ran);
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 2u);
}

// Test that reveal locks work as expected.
TEST_F(CocoaImmersiveModeControllerTest, RevealLock) {
  // Controller under test.
  auto immersive_mode_controller = std::make_unique<ImmersiveModeController>(
      browser(), overlay(), base::DoNothing());
  immersive_mode_controller->Enable();

  // Autohide top chrome.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  EXPECT_EQ(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      0);

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
  EXPECT_EQ(
      browser()
          .titlebarAccessoryViewControllers.firstObject.fullScreenMinHeight,
      0);
}

// Test ImmersiveModeController titlebar frame KVO.
TEST_F(CocoaImmersiveModeControllerTest, TitlebarObserver) {
  // Create a fake NSToolbarFullScreenWindow and associated views.
  base::scoped_nsobject<NSView> titlebar_container_view([[NSView alloc]
      initWithFrame:NSMakeRect(0, kOverlayViewHeight, kOverlayViewWidth,
                               kOverlayViewHeight)]);

  base::scoped_nsobject<NSWindow> fullscreen_window([[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, kOverlayViewWidth, kBrowserHeight)
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  fullscreen_window.get().releasedWhenClosed = NO;
  [fullscreen_window.get().contentView addSubview:titlebar_container_view];
  [fullscreen_window orderBack:nil];

  auto immersive_mode_controller = std::make_unique<ImmersiveModeController>(
      browser(), overlay(), base::DoNothing());
  base::WeakPtrFactory<ImmersiveModeController> weak_ptr_factory(
      immersive_mode_controller.get());

  // Grab the content view from the controller and add it to the test
  // `titlebar_container_view`.
  BridgedContentView* overlay_view =
      immersive_mode_controller->overlay_content_view();
  [titlebar_container_view addSubview:overlay_view];
  overlay_view.frame = NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight);

  // Create a titlebar observer. This is the class under test.
  base::scoped_nsobject<ImmersiveModeTitlebarObserver> titlebar_observer(
      [[ImmersiveModeTitlebarObserver alloc]
             initWithController:weak_ptr_factory.GetWeakPtr()
          titlebarContainerView:titlebar_container_view]);

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
    EXPECT_EQ(overlay().frame.origin.y, -kOverlayViewHeight);

    // Remove the clip and make sure the overlay window moves back.
    [titlebar_container_view
        setFrame:NSMakeRect(0, kOverlayViewHeight, kOverlayViewWidth,
                            kOverlayViewHeight)];
    EXPECT_EQ(overlay().frame.origin.y, kOverlayViewHeight);
  }

  [titlebar_container_view removeObserver:titlebar_observer
                               forKeyPath:@"frame"];

  [fullscreen_window close];
  fullscreen_window.reset();
}

// Test ImmersiveModeController toolbar visibility.
TEST_F(CocoaImmersiveModeControllerTest, ToolbarVisibility) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(
          browser(), overlay(), tab_overlay(), base::DoNothing());
  immersive_mode_controller->Enable();

  // The controller will be hidden until the fullscreen transition is complete.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAlways);
  EXPECT_TRUE(browser().titlebarAccessoryViewControllers.firstObject.hidden);
  immersive_mode_controller->FullscreenTransitionCompleted();
  EXPECT_FALSE(browser().titlebarAccessoryViewControllers.firstObject.hidden);

  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_TRUE(browser().titlebarAccessoryViewControllers.firstObject.hidden);

  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  EXPECT_FALSE(browser().titlebarAccessoryViewControllers.firstObject.hidden);
}

// Test ImmersiveModeTabbedController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, Tabbed) {
  bool view_will_appear_ran = false;
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(
          browser(), overlay(), tab_overlay(),
          base::BindOnce(
              [](bool* view_will_appear_ran) { *view_will_appear_ran = true; },
              &view_will_appear_ran));
  immersive_mode_controller->Enable();
  EXPECT_TRUE(view_will_appear_ran);

  // TODO(https://crbug.com/1426944): Enable() does not add the controller. It
  // will be added / removed from the view controller tree during
  // UpdateToolbarVisibility(). Remove this comment and update the test once the
  // bug has been resolved.
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 2u);
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAlways);
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 3u);
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 2u);
}

// Test ImmersiveModeTabbedController reveal lock tests.
TEST_F(CocoaImmersiveModeControllerTest, TabbedRevealLock) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedController>(
          browser(), overlay(), tab_overlay(), base::DoNothing());
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
      std::make_unique<ImmersiveModeTabbedController>(
          browser(), overlay(), tab_overlay(), base::DoNothing());
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

}  // namespace remote_cocoa
