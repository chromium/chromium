// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSMENU_ADDITIONS_H_
#define UI_BASE_COCOA_NSMENU_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

@interface NSMenu (ChromeAdditions)

// Searches the menu and its submenus for the item with the keyboard
// equivalent matching `event`. Returns nil if no corresponding
// menu item exists.
- (NSMenuItem*)cr_menuItemForKeyEquivalentEvent:(NSEvent*)event;

@end

#endif  // UI_BASE_COCOA_NSMENU_ADDITIONS_H_
