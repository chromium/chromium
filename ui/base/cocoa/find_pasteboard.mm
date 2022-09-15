// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/find_pasteboard.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

NSString* kFindPasteboardChangedNotification =
    @"kFindPasteboardChangedNotification_Chrome";

@implementation FindPasteboard

+ (FindPasteboard*)sharedInstance {
  static FindPasteboard* instance = nil;
  if (!instance) {
    instance = [[FindPasteboard alloc] init];
  }
  return instance;
}

- (instancetype)init {
  if ((self = [super init])) {
    _findText.reset([[NSString alloc] init]);

    // Check if the text in the findboard has changed on app activate.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(loadTextFromPasteboard:)
               name:NSApplicationDidBecomeActiveNotification
             object:nil];
    [self loadTextFromPasteboard:nil];
  }
  return self;
}

- (void)dealloc {
  // Since this is a singleton, this should only be executed in test code.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSPasteboard*)findPboard {
  return [NSPasteboard pasteboardWithName:NSPasteboardNameFind];
}

- (void)loadTextFromPasteboard:(NSNotification*)notification {
  NSPasteboard* findPboard = [self findPboard];
  if ([[findPboard types] containsObject:NSStringPboardType])
    [self setFindText:[findPboard stringForType:NSStringPboardType]];
}

- (NSString*)findText {
  return _findText;
}

- (void)setFindText:(NSString*)newText {
  DCHECK(newText);
  if (!newText)
    return;

  DCHECK([NSThread isMainThread]);

  BOOL needToSendNotification = ![_findText.get() isEqualToString:newText];
  if (needToSendNotification) {
    _findText.reset([newText copy]);
    NSPasteboard* findPboard = [self findPboard];
    [findPboard declareTypes:@[ NSStringPboardType ] owner:nil];
    [findPboard setString:_findText.get() forType:NSStringPboardType];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kFindPasteboardChangedNotification
                      object:self];
  }
}

@end
