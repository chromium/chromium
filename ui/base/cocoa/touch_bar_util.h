// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOUCH_BAR_UTIL_H_
#define UI_BASE_COCOA_TOUCH_BAR_UTIL_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

namespace ui {

// Creates a touch bar identifier with the given |id|.
COMPONENT_EXPORT(UI_BASE) NSString* GetTouchBarId(NSString* touch_bar_id);

// Creates a touch Bar item identifier.
COMPONENT_EXPORT(UI_BASE)
NSString* GetTouchBarItemId(NSString* touch_bar_id, NSString* item_id);

}  // namespace ui

#endif  // UI_BASE_COCOA_TOUCH_BAR_UTIL_H_
