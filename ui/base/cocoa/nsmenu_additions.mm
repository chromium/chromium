// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsmenu_additions.h"

#include "base/check.h"
#include "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/nsmenuitem_additions.h"

namespace {

typedef void (^PreSearchBlock)(void);

PreSearchBlock g_pre_search_block;

NSMenuItem* MenuItemForKeyEquivalentEventInMenu(NSEvent* event, NSMenu* menu) {
  NSMenuItem* result = nil;

  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = MenuItemForKeyEquivalentEventInMenu(event, submenu);
    } else if ([item cr_firesForKeyEquivalentEvent:event]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
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

@end
