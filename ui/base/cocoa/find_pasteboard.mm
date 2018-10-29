// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/find_pasteboard.h"

#include "base/logging.h"
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
    findText_.reset([[NSString alloc] init]);

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
  return [NSPasteboard pasteboardWithName:NSFindPboard];
}

- (void)loadTextFromPasteboard:(NSNotification*)notification {
  NSPasteboard* findPboard = [self findPboard];
  if ([[findPboard types] containsObject:NSStringPboardType])
    [self setFindText:[findPboard stringForType:NSStringPboardType]];
}

- (NSString*)findText {
  return findText_;
}

- (void)setFindText:(NSString*)newText {
  DCHECK(newText);
  if (!newText)
    return;

  DCHECK([NSThread isMainThread]);

  BOOL needToSendNotification = ![findText_.get() isEqualToString:newText];
  if (needToSendNotification) {
    findText_.reset([newText copy]);
    NSPasteboard* findPboard = [self findPboard];
    [findPboard declareTypes:@[ NSStringPboardType ] owner:nil];
    [findPboard setString:findText_.get() forType:NSStringPboardType];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kFindPasteboardChangedNotification
                      object:self];
  }
}

@end

base::string16 GetFindPboardText() {
  return base::SysNSStringToUTF16([[FindPasteboard sharedInstance] findText]);
}
