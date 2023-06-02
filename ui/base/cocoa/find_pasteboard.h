// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_FIND_PASTEBOARD_H_
#define UI_BASE_COCOA_FIND_PASTEBOARD_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

COMPONENT_EXPORT(UI_BASE) extern NSString* kFindPasteboardChangedNotification;

// Manages the find pasteboard. Use this to copy text to the find pasteboard,
// to get the text currently on the find pasteboard, and to receive
// notifications when the text on the find pasteboard has changed. You should
// always use this class instead of accessing
// [NSPasteboard pasteboardWithName:NSPasteboardNameFind] directly.
//
// This is not thread-safe and must be used on the main thread.
//
// This is supposed to be a singleton.
COMPONENT_EXPORT(UI_BASE)
@interface FindPasteboard : NSObject

// Returns the singleton instance of this class.
+ (FindPasteboard*)sharedInstance;

// Returns the current find text. This is never nil; if there is no text on the
// find pasteboard, this returns an empty string.
- (NSString*)findText;

// Sets the current find text to `newText` and sends a
// `kFindPasteboardChangedNotification` to the default notification center if
// it the new text different from the current text. `newText` must not be nil.
- (void)setFindText:(NSString*)newText;
@end

@interface FindPasteboard (TestingAPI)
- (void)loadTextFromPasteboard:(NSNotification*)notification;

// This method is meant to be overridden in tests.
- (NSPasteboard*)findPasteboard;
@end

#endif  // UI_BASE_COCOA_FIND_PASTEBOARD_H_
