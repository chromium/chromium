// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_
#define UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_

#import <Foundation/Foundation.h>

#include "base/component_export.h"
#include "ui/base/accelerators/accelerator.h"

COMPONENT_EXPORT(UI_BASE)
@interface KeyEquivalentAndModifierMask : NSObject

@property(readonly) NSString* keyEquivalent;
@property(readonly) NSUInteger modifierMask;

@end

namespace ui {

// On macOS, accelerators are primarily handled by the main menu. Most
// accelerators have an associated NSMenuItem. Each NSMenuItem is specified with
// a `key_equivalent` and `modifier_mask`. This function takes a ui::Accelerator
// and returns the associated `key_equivalent` and `modifier_mask`.
COMPONENT_EXPORT(UI_BASE)
KeyEquivalentAndModifierMask* GetKeyEquivalentAndModifierMaskFromAccelerator(
    const ui::Accelerator& accelerator);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_
