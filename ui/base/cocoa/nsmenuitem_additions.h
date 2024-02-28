// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_
#define UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

@interface NSMenuItem (ChromeAdditions)

// Returns YES if the menu item is enabled and a call to
// [menu performKeyEquivalent:`event`] would cause the
// menu item to fire.
- (BOOL)cr_firesForKeyEquivalentEvent:(NSEvent*)event;

// Convenience method for setting a menu item's key equivalent.
- (void)cr_setKeyEquivalent:(NSString*)aString
               modifierMask:(NSEventModifierFlags)mask;

// Convenience method for clearing a menu item's key equivalent. After calling,
// the item has no keyEquivalent string or mask.
- (void)cr_clearKeyEquivalent;

@end

namespace ui::cocoa {

// Used by tests to set internal state without having to change global input
// source.
void COMPONENT_EXPORT(UI_BASE)
    SetIsInputSourceCommandQwertyForTesting(bool is_command_qwerty);

void COMPONENT_EXPORT(UI_BASE)
    SetIsInputSourceDvorakRightOrLeftForTesting(bool is_dvorak_right_or_left);

void COMPONENT_EXPORT(UI_BASE)
    SetIsInputSourceCommandHebrewForTesting(bool is_command_hebrew);

void COMPONENT_EXPORT(UI_BASE)
    SetIsInputSourceCommandArabicForTesting(bool is_command_arabic);

// Returns whether the named keyboard layout has the command-qwerty behavior,
// meaning that the layout acts as though it was QWERTY when the command key is
// held.
bool COMPONENT_EXPORT(UI_BASE)
    IsKeyboardLayoutCommandQwerty(NSString* layout_id);

// Returns a suitable keyboard shortcut modifier mask for `event`. In
// particular, NSEventModifierFlagFunction may be present in the event's
// modifiers but it may not indicate the user is pressing the Function key (it
// exists, for example, when pressing a function key like Up Arrow). This
// distinction matters when evaluating a key event as a possible keyboard
// shortcut.
NSUInteger COMPONENT_EXPORT(UI_BASE) ModifierMaskForKeyEvent(NSEvent* event);

}  // namespace ui::cocoa

#endif  // UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_
