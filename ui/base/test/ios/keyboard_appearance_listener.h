// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_IOS_KEYBOARD_APPEARANCE_LISTENER_H_
#define UI_BASE_TEST_IOS_KEYBOARD_APPEARANCE_LISTENER_H_

#import <UIKit/UIKit.h>

// Listener to observe the keyboard coming up or down.
@interface KeyboardAppearanceListener : NSObject

// Returns YES if the keyboard is currently visible.
@property(nonatomic, readonly, getter=isKeyboardVisible) bool keyboardVisible;

@end

#endif  // UI_BASE_TEST_IOS_KEYBOARD_APPEARANCE_LISTENER_H_
