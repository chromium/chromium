// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller_cocoa.h"
#include "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "ui/base/cocoa/window_size_constants.h"
#import "ui/base/test/cocoa_helper.h"

@interface NSTitlebarAccessoryViewController (Chrome)
- (void)setRevealAmount:(double)input;
@end

namespace {

constexpr float kBrowserHeight = 200;
constexpr float kBrowserWidth = 400;
constexpr float kOverlayViewHeight = 100;
constexpr float kOverlayViewWidth = kBrowserWidth;
constexpr float kTabOverlayViewHeight = 50;
constexpr float kTabOverlayViewWidth = kBrowserWidth;
constexpr float kPopupHeight = 100;
constexpr float kPopupWidth = kPopupHeight;
constexpr float kTitlebarHeight = 28;

NativeWidgetMacNSWindow* CreateNativeWidgetMacNSWindow(
    CGFloat width,
    CGFloat height,
    NSWindowStyleMask style_mask = NSWindowStyleMaskBorderless) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:style_mask
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.releasedWhenClosed = NO;
  [window setFrame:NSMakeRect(0, 0, width, height) display:YES];
  window.contentView = [[BridgedContentView alloc] initWithBridge:nullptr
                                                           bounds:gfx::Rect()];
  [window.contentView setFrame:NSMakeRect(0, 0, width, height)];

  return window;
}

}  // namespace

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
    browser_ = CreateNativeWidgetMacNSWindow(
        kBrowserWidth, kBrowserHeight,
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);
    [browser_ orderBack:nil];

    // Create a blank overlay window.
    overlay_ =
        CreateNativeWidgetMacNSWindow(kOverlayViewWidth, kOverlayViewHeight);
    [browser_ addChildWindow:overlay_ ordered:NSWindowAbove];
    EXPECT_EQ(overlay_.isVisible, YES);

    // Create a blank tab overlay window as a child of overlay window.
    tab_overlay_ = CreateNativeWidgetMacNSWindow(kTabOverlayViewWidth,
                                                 kTabOverlayViewHeight);
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

  NativeWidgetMacNSWindow* browser() { return browser_; }
  NativeWidgetMacNSWindow* overlay() { return overlay_; }
  NativeWidgetMacNSWindow* tab_overlay() { return tab_overlay_; }

 private:
  NativeWidgetMacNSWindow* __strong browser_;
  NativeWidgetMacNSWindow* __strong overlay_;
  NativeWidgetMacNSWindow* __strong tab_overlay_;
};

// Test ImmersiveModeController construction and destruction.
TEST_F(CocoaImmersiveModeControllerTest, ImmersiveModeController) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeControllerCocoa>(browser(), overlay());
  immersive_mode_controller->Init();
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 1u);
}

// Test that reveal locks work as expected.
TEST_F(CocoaImmersiveModeControllerTest, RevealLock) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeControllerCocoa>(browser(), overlay());
  immersive_mode_controller->Init();

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

// Test KVO on ImmersiveModeController titlebar container view's frame.
TEST_F(CocoaImmersiveModeControllerTest, TitlebarContainerViewObserver) {
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
      std::make_unique<ImmersiveModeControllerCocoa>(browser(), overlay());
  base::WeakPtrFactory<ImmersiveModeControllerCocoa> weak_ptr_factory(
      immersive_mode_controller.get());

  // Grab the content view from the controller and add it to the test
  // `titlebar_container_view`.
  BridgedContentView* overlay_view =
      immersive_mode_controller->overlay_content_view();
  [titlebar_container_view addSubview:overlay_view];
  overlay_view.frame = NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight);

  // Create a titlebar observer. This is the class under test.
  [[maybe_unused]] ImmersiveModeTitlebarObserver* titlebar_observer =
      [[ImmersiveModeTitlebarObserver alloc]
             initWithController:weak_ptr_factory.GetWeakPtr()
          titlebarContainerView:titlebar_container_view];

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

  [fullscreen_window close];
  fullscreen_window = nil;
}

// Test that IsReveal() reflects the toolbar visibility.
TEST_F(CocoaImmersiveModeControllerTest, IsRevealed) {
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
      std::make_unique<ImmersiveModeControllerCocoa>(browser(), overlay());
  base::WeakPtrFactory<ImmersiveModeControllerCocoa> weak_ptr_factory(
      immersive_mode_controller.get());

  NSTitlebarAccessoryViewController* titlebar_view_controller =
      immersive_mode_controller
          ->immersive_mode_titlebar_view_controller_for_testing();

  // Grab the content view from the controller and add it to the test
  // `titlebar_container_view`.
  BridgedContentView* overlay_view =
      immersive_mode_controller->overlay_content_view();
  [titlebar_container_view addSubview:overlay_view];
  overlay_view.frame = NSMakeRect(0, 0, kOverlayViewWidth, kOverlayViewHeight);

  [titlebar_view_controller setRevealAmount:0];
  titlebar_view_controller.fullScreenMinHeight = 0;
  EXPECT_FALSE(immersive_mode_controller->IsToolbarRevealed());

  [titlebar_view_controller setRevealAmount:1];
  EXPECT_TRUE(immersive_mode_controller->IsToolbarRevealed());

  [titlebar_view_controller setRevealAmount:0];
  titlebar_view_controller.fullScreenMinHeight = 100;
  EXPECT_TRUE(immersive_mode_controller->IsToolbarRevealed());

  [fullscreen_window close];
  fullscreen_window = nil;
}

// Test ImmersiveModeController toolbar visibility.
TEST_F(CocoaImmersiveModeControllerTest, ToolbarVisibility) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedControllerCocoa>(browser(), overlay(),
                                                           tab_overlay());
  immersive_mode_controller->Init();

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
      std::make_unique<ImmersiveModeTabbedControllerCocoa>(browser(), overlay(),
                                                           tab_overlay());
  immersive_mode_controller->Init();

  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 2u);
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_EQ(browser().titlebarAccessoryViewControllers.count, 1u);
}

// Test ImmersiveModeTabbedController reveal lock tests.
TEST_F(CocoaImmersiveModeControllerTest, TabbedRevealLock) {
  // Controller under test.
  auto immersive_mode_controller =
      std::make_unique<ImmersiveModeTabbedControllerCocoa>(browser(), overlay(),
                                                           tab_overlay());
  immersive_mode_controller->Init();
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
      std::make_unique<ImmersiveModeTabbedControllerCocoa>(browser(), overlay(),
                                                           tab_overlay());
  immersive_mode_controller->Init();
  immersive_mode_controller->FullscreenTransitionCompleted();

  // Autohide top chrome.
  immersive_mode_controller->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);

  // Create a popup.
  NSWindow* popup = CreateNativeWidgetMacNSWindow(kPopupWidth, kPopupHeight);
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
      std::make_unique<ImmersiveModeTabbedControllerCocoa>(browser(), overlay(),
                                                           tab_overlay());
  immersive_mode_controller->Init();
  immersive_mode_controller->FullscreenTransitionCompleted();

  // Create a popup.
  NSWindow* popup = CreateNativeWidgetMacNSWindow(kPopupWidth, kPopupHeight);
  EXPECT_EQ(immersive_mode_controller->reveal_lock_count(), 0);

  // Add the popup as a child of overlay.
  [overlay() addChildWindow:popup ordered:NSWindowAbove];

  // Make sure the tab overlay window stays on z-order top.
  EXPECT_EQ(overlay().childWindows.lastObject, tab_overlay());

  [popup close];
}

class MockImmersiveModeTabbedControllerCocoa
    : public ImmersiveModeTabbedControllerCocoa {
 public:
  MockImmersiveModeTabbedControllerCocoa(
      NativeWidgetMacNSWindow* browser_window,
      NativeWidgetMacNSWindow* overlay_window,
      NativeWidgetMacNSWindow* tab_window)
      : ImmersiveModeTabbedControllerCocoa(browser_window,
                                           overlay_window,
                                           tab_window) {}
  MOCK_METHOD(void, RevealLock, (), (override));
  MOCK_METHOD(void, RevealUnlock, (), (override));
};

TEST_F(CocoaImmersiveModeControllerTest, NoRevealUnlockDuringChildReordering) {
  // Controller under test.
  testing::StrictMock<MockImmersiveModeTabbedControllerCocoa>
      immersive_mode_controller(browser(), overlay(), tab_overlay());
  immersive_mode_controller.Init();
  immersive_mode_controller.FullscreenTransitionCompleted();

  // Create a popup.
  NSWindow* popup = CreateNativeWidgetMacNSWindow(100, 100);

  // Add the popup as a child of overlay.
  // Reveal lock once on child add.
  EXPECT_CALL(immersive_mode_controller, RevealLock()).Times(1);
  [overlay() addChildWindow:popup ordered:NSWindowAbove];

  // During re-ordering, no reveal lock or unlock should happen.
  [overlay() orderWindowByShuffling:NSWindowAbove relativeTo:0];

  // Reveal unlock once on child removal.
  EXPECT_CALL(immersive_mode_controller, RevealUnlock()).Times(1);
  [popup close];
}

}  // namespace remote_cocoa
