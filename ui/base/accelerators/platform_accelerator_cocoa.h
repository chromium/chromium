// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_
#define UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_

#import <Foundation/Foundation.h>

#include "base/component_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

// Returns |true| if there is an associated NSMenuItem, and populates output
// variables |key_equivalent| and |modifier_mask|.
//
// On macOS, accelerators are primarily handled by the main menu. Most
// accelerators have an associated NSMenuItem. Each NSMenuItem is specified with
// a |key_equivalent| and |modifier_mask|. This function takes a ui::Accelerator
// and returns the associated |key_equivalent| and |modifier_mask|.
COMPONENT_EXPORT(UI_BASE)
void GetKeyEquivalentAndModifierMaskFromAccelerator(
    const ui::Accelerator& accelerator,
    NSString** key_equivalent,
    NSUInteger* modifier_mask);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_PLATFORM_ACCELERATOR_COCOA_H_
