// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsmenu_additions.h"

#include "base/check.h"
#import "ui/base/cocoa/nsmenuitem_additions.h"

namespace {

void (^g_pre_search_block)(void);

NSMenuItem* MenuItemForKeyEquivalentEventInMenu(NSEvent* event, NSMenu* menu) {
  NSMenuItem* result = nil;

  for (NSMenuItem* item in menu.itemArray) {
    NSMenu* submenu = item.submenu;
    if (submenu) {
      if (submenu != NSApp.servicesMenu) {
        result = MenuItemForKeyEquivalentEventInMenu(event, submenu);
      }
    } else if ([item cr_firesForKeyEquivalentEvent:event]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}

// Searches |menu| and its submenus for a NSMenuItem with |tag|.
// Returns the menu item or nil if no menu item matches.
NSMenuItem* MenuItemWithTagInMenu(int tag, NSMenu* menu) {
  for (NSMenuItem* item in menu.itemArray) {
    if (item.tag == tag) {
      return item;
    }

    if (item.hasSubmenu) {
      NSMenuItem* the_item = MenuItemWithTagInMenu(tag, item.submenu);
      if (the_item != nil)
        return the_item;
    }
  }

  return nil;
}

NSMenuItem* MenuItemWithTag(int tag) {
  NSMenu* mainMenu = NSApp.mainMenu;

  // Validate menu items before searching.
  [mainMenu update];

  return MenuItemWithTagInMenu(tag, mainMenu);
}

}  // namespace

@implementation NSMenu (ChromeAdditions)

+ (void)cr_setMenuItemForKeyEquivalentEventPreSearchBlock:
    (void (^)(void))block {
  if (block != nil)
    CHECK(g_pre_search_block == nil);
  g_pre_search_block = block;
}

- (NSMenuItem*)cr_menuItemForKeyEquivalentEvent:(NSEvent*)event {
  if ([event type] != NSEventTypeKeyDown)
    return nil;

  if (g_pre_search_block) {
    g_pre_search_block();
  }

  // Validate menu items before searching.
  [self update];

  return MenuItemForKeyEquivalentEventInMenu(event, self);
}

+ (BOOL)flashMenuForChromeCommand:(int)chromeCommand {
  NSMenuItem* menuItem = MenuItemWithTag(chromeCommand);
  if (menuItem == nil)
    return NO;

  // Swap out the menu item's existing target/action for a fake pair,
  // so that we can flash the menu item without executing anything.
  id origTarget = menuItem.target;
  SEL origAction = menuItem.action;
  menuItem.target = self;
  menuItem.action = @selector(cr_executeDummyCommand:);

  // -performActionForItemAtIndex: is documented as triggering highlighting in
  // the menu bar as well as sending out appropriate accessibility notifications
  // indicating the item was selected.
  NSMenu* owningMenu = menuItem.menu;
  [owningMenu performActionForItemAtIndex:[owningMenu indexOfItem:menuItem]];

  // Restore.
  menuItem.target = origTarget;
  menuItem.action = origAction;

  return YES;
}

+ (void)cr_executeDummyCommand:(id)sender {
}

@end
