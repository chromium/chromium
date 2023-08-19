// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_OBSERVER_H_
#define UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/component_export.h"

// An observer for macOS's accessibility pointer size user preference.
COMPONENT_EXPORT(UI_BASE)
@interface CursorAccessibilityScaleFactorObserver : NSObject
// `handler` is invoked when the user preference changes.
- (instancetype)initWithHandler:(void (^)())handler;
@end

#endif  // UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_OBSERVER_H_
