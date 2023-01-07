// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/accelerators/platform_accelerator_cocoa.h"

#import <AppKit/AppKit.h>

#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace ui {

void GetKeyEquivalentAndModifierMaskFromAccelerator(
    const ui::Accelerator& accelerator,
    NSString** key_equivalent,
    NSUInteger* modifier_mask) {
  DCHECK_NE(ui::VKEY_UNKNOWN, accelerator.key_code());
  NSUInteger cocoa_modifiers = 0;
  if (accelerator.IsShiftDown())
    cocoa_modifiers |= NSEventModifierFlagShift;
  if (accelerator.IsCtrlDown())
    cocoa_modifiers |= NSEventModifierFlagControl;
  if (accelerator.IsAltDown())
    cocoa_modifiers |= NSEventModifierFlagOption;
  if (accelerator.IsCmdDown())
    cocoa_modifiers |= NSEventModifierFlagCommand;
  if (accelerator.IsFunctionDown())
    cocoa_modifiers |= NSEventModifierFlagFunction;

  unichar shifted_character;
  unichar character;
  int result = ui::MacKeyCodeForWindowsKeyCode(
      accelerator.key_code(), cocoa_modifiers, &shifted_character, &character);
  DCHECK_NE(result, -1);

  // If the key equivalent is itself shifted, then drop Shift from the modifier
  // flags, otherwise Shift will be required. E.g., curly braces and plus are
  // both inherently shifted, so the key equivalents shouldn't require Shift.
  if (shifted_character != character)
    cocoa_modifiers &= ~NSEventModifierFlagShift;
  *key_equivalent = [NSString stringWithFormat:@"%C", shifted_character];
  *modifier_mask = cocoa_modifiers;
}

}  // namespace ui
