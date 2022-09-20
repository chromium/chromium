// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSMENU_ADDITIONS_H_
#define UI_BASE_COCOA_NSMENU_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

@interface NSMenu (ChromeAdditions)

// Sets a block that cr_menuItemForKeyEquivalentEvent: calls before
// beginning its search for a matching menu item. Useful for code
// outside of /content to arrange for code execution (to perform custom
// menu item updates, for example) before the search begins.
//
// This method does not support multiple pre-search blocks. It will
// CHECK() if called after a block has already been set.
+ (void)cr_setMenuItemForKeyEquivalentEventPreSearchBlock:(void (^)(void))block;

// Searches the menu and its submenus for the item with the keyboard
// equivalent matching `event`. Returns nil if no corresponding
// menu item exists.
- (NSMenuItem*)cr_menuItemForKeyEquivalentEvent:(NSEvent*)event;

// Locates the menu item corresponding to `chromeCommand` and flashes
// its owning menu title in the menu bar (without executing the command).
// Also causes VoiceOver to speak the menu item. Returns NO if the
// command wasn't found in the menus.
+ (BOOL)flashMenuForChromeCommand:(int)chromeCommand;

@end

#endif  // UI_BASE_COCOA_NSMENU_ADDITIONS_H_
