// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_MACOS27_SDK_H_
#define UI_BASE_COCOA_MACOS27_SDK_H_

// Forward-define an NSMenuItem property used throughout Chromium code. Delete
// when Chromium builds against the macOS 27 SDK.

#if !defined(__MAC_27_0)

#import <AppKit/AppKit.h>

@interface NSMenuItem (macOS27SDK)

typedef NS_ENUM(NSInteger, NSMenuItemImageVisibility) {
  NSMenuItemImageVisibilityAutomatic = 0,
  NSMenuItemImageVisibilityVisible = 1,
  NSMenuItemImageVisibilityHidden = 2
} API_AVAILABLE(macos(27.0));

@property NSMenuItemImageVisibility preferredImageVisibility API_AVAILABLE(
    macos(27.0));

@end

#endif

#endif  // UI_BASE_COCOA_MACOS27_SDK_H_
