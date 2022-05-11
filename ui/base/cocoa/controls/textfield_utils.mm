// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/controls/textfield_utils.h"

@implementation TextFieldUtils

+ (NSTextField*)labelWithString:(NSString*)text {
  NSTextField* textfield =
      [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
  [textfield setBezeled:NO];
  [textfield setDrawsBackground:NO];
  [textfield setEditable:NO];
  [textfield setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [textfield setStringValue:text];
  [textfield setAlignment:NSTextAlignmentNatural];
  return textfield;
}

@end
