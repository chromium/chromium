// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_
#define UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

@interface NSMenuItem (ChromeAdditions)

// Returns true exactly if the menu item would fire if it would be put into
// a menu and then |menu performKeyEquivalent:event| was called.
// This method always returns NO if the menu item is not enabled.
- (BOOL)cr_firesForKeyEvent:(NSEvent*)event;

@end

namespace ui {
namespace cocoa {

// Used by tests to set internal state without having to change global input
// source.
void COMPONENT_EXPORT(UI_BASE)
    SetIsInputSourceCommandQwertyForTesting(bool is_command_qwerty);

// Returns whether the named keyboard layout has the command-qwerty behavior,
// meaning that the layout acts as though it was QWERTY when the command key is
// held.
bool COMPONENT_EXPORT(UI_BASE)
    IsKeyboardLayoutCommandQwerty(NSString* layout_id);

}  // namespace cocoa
}  // namespace ui

#endif  // UI_BASE_COCOA_NSMENUITEM_ADDITIONS_H_
