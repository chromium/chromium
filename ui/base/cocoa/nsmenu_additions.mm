// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsmenu_additions.h"

#import "ui/base/cocoa/nsmenuitem_additions.h"

namespace {

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

- (NSMenuItem*)cr_menuItemForKeyEquivalentEvent:(NSEvent*)event {
  if ([event type] != NSEventTypeKeyDown)
    return nil;

  // Validate menu items before searching.
  [self update];

  return MenuItemForKeyEquivalentEventInMenu(event, self);
}

@end
