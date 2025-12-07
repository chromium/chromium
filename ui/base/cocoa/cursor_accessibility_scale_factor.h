// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_H_
#define UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_H_

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif  // __OBJC__

#include "base/component_export.h"

// macOS has an accessibility pointer size user preference. The OS renders
// larger Chrome and web content cursors using this scale factor (1.0 - 4.0).
//
// Call `GetCursorAccessibilityScaleFactor` to access the value in a one-time
// manner, or use `CursorAccessibilityScaleFactorObserver` to get a callback
// whenever the value changes.

namespace ui {

// Returns the current accessibility pointer size user preference.
COMPONENT_EXPORT(UI_BASE)
float GetCursorAccessibilityScaleFactor();

}  // namespace ui

#ifdef __OBJC__
// An class to observe macOS's accessibility pointer size user preference. This
// will call the registered observers every time that preference value changes.
//
// Be sure to use this class in the browser process. Notifications about
// NSUserDefaults changes are blocked by the sandbox, so this class will not
// work in renderer or other such sandboxed processes.
COMPONENT_EXPORT(UI_BASE)
@interface CursorAccessibilityScaleFactorNotifier : NSObject

@property(class, readonly)
    CursorAccessibilityScaleFactorNotifier* sharedNotifier;

// Adds an observer block that is called when the accessibility pointer size
// user preference changes. Returns an opaque token to represent the
// observation; call -removeObserver: to remove the observation when it is no
// longer needed.
- (id<NSObject>)addObserver:(void (^)())observer;

// Removes the observer block corresponding to the token.
- (void)removeObserver:(id<NSObject>)token;

@end

#endif  // __OBJC__

#endif  // UI_BASE_COCOA_CURSOR_ACCESSIBILITY_SCALE_FACTOR_H_
