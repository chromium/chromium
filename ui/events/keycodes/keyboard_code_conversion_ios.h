// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_IOS_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_IOS_H_

#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

#if BUILDFLAG(IS_IOS_TVOS)
#import <UIKit/UIKit.h>
#else
@class BEKeyEntry;
#endif

namespace ui {

enum class DomCode : uint32_t;

#if BUILDFLAG(IS_IOS_TVOS)

EVENTS_BASE_EXPORT KeyboardCode
KeyboardCodeFromUIKeyCode(UIKeyboardHIDUsage key_code);

EVENTS_BASE_EXPORT DomKey DomKeyFromKeyboardCode(UIPress* press,
                                                 KeyboardCode key_code);

EVENTS_BASE_EXPORT DomCode DomCodeFromUIPress(UIPress* press,
                                              KeyboardCode key_code);

#else

EVENTS_BASE_EXPORT DomCode DomCodeFromBEKeyEntry(BEKeyEntry* event);

EVENTS_BASE_EXPORT DomKey DomKeyFromBEKeyEntry(BEKeyEntry* event);

#endif

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_IOS_H_
