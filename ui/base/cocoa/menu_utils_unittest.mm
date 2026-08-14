// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/menu_utils.h"

#import <AppKit/AppKit.h>
#import <objc/objc-class.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#import "ui/base/test/cocoa_helper.h"
#include "ui/gfx/geometry/point.h"

@interface DummyMenu : NSObject
+ (void)popUpContextMenu:(NSMenu*)menu
               withEvent:(NSEvent*)event
                 forView:(NSView*)view;
@end

@implementation DummyMenu
+ (void)popUpContextMenu:(NSMenu*)menu
               withEvent:(NSEvent*)event
                 forView:(NSView*)view {
  // While the menu is active, anchor location should be set.
  EXPECT_TRUE(ui::GetActiveCocoaMenuAnchorLocation().has_value());
}
@end

namespace ui {

namespace {

class MenuUtilsTest : public CocoaTest {};

TEST_F(MenuUtilsTest, ShowContextMenuActiveAnchorLocation) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  EXPECT_EQ(std::nullopt, GetActiveCocoaMenuAnchorLocation());

  base::apple::ScopedObjCClassSwizzler swizzler(
      object_getClass([NSMenu class]), object_getClass([DummyMenu class]),
      @selector(popUpContextMenu:withEvent:forView:));

  NSWindow* window = test_window();
  gfx::Point anchor_point(100, 200);
  NSEvent* event = EventForPositioningContextMenu(anchor_point, window);
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Test Menu"];

  // 1. Show context menu without ElementContext (empty context).
  ShowContextMenu(menu, event, [window contentView],
                  /*allow_nested_tasks=*/true, ElementContext());

  // Anchor location is cleared synchronously as soon as menu returns.
  EXPECT_EQ(std::nullopt, GetActiveCocoaMenuAnchorLocation());

  // 2. Show context menu with a valid ElementContext.
  ElementContext valid_context =
      ElementContext::CreateFakeContextForTesting(this);
  ShowContextMenu(menu, event, [window contentView],
                  /*allow_nested_tasks=*/true, valid_context);

  EXPECT_EQ(std::nullopt, GetActiveCocoaMenuAnchorLocation());
}

}  // namespace

}  // namespace ui
