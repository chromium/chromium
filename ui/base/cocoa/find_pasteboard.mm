// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/find_pasteboard.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

NSString* kFindPasteboardChangedNotification =
    @"kFindPasteboardChangedNotification_Chrome";

@implementation FindPasteboard {
  NSString* _findText;
}

+ (FindPasteboard*)sharedInstance {
  static FindPasteboard* instance = nil;
  if (!instance) {
    instance = [[FindPasteboard alloc] init];
  }
  return instance;
}

- (instancetype)init {
  if ((self = [super init])) {
    _findText = @"";

    // Check if the text in the find pasteboard has changed on app activate.
    [NSNotificationCenter.defaultCenter
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
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (NSPasteboard*)findPasteboard {
  return [NSPasteboard pasteboardWithName:NSPasteboardNameFind];
}

- (void)loadTextFromPasteboard:(NSNotification*)notification {
  NSPasteboard* findPasteboard = [self findPasteboard];
  NSArray* objects = [findPasteboard readObjectsForClasses:@[ [NSString class] ]
                                                   options:nil];
  if (objects.count) {
    [self setFindText:objects.firstObject];
  }
}

- (NSString*)findText {
  return _findText;
}

- (void)setFindText:(NSString*)newText {
  DCHECK(newText);
  if (!newText) {
    return;
  }

  DCHECK(NSThread.isMainThread);

  BOOL textChanged = ![_findText isEqualToString:newText];
  if (textChanged) {
    _findText = [newText copy];

    NSPasteboard* findPasteboard = [self findPasteboard];
    [findPasteboard clearContents];
    [findPasteboard writeObjects:@[ _findText ]];

    [NSNotificationCenter.defaultCenter
        postNotificationName:kFindPasteboardChangedNotification
                      object:self];
  }
}

@end
