// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/controls/button_utils.h"

@implementation ButtonUtils

+ (NSButton*)buttonWithTitle:(NSString*)title
                      action:(SEL)action
                      target:(id)target {
  NSButton* button = [[[NSButton alloc] initWithFrame:NSZeroRect] autorelease];
  [button setAction:action];
  [button setButtonType:NSMomentaryLightButton];
  [button setFocusRingType:NSFocusRingTypeExterior];
  [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [button setTarget:target];
  [button setTitle:title];
  [[button cell] setBezelStyle:NSRoundedBezelStyle];
  return button;
}

+ (NSButton*)checkboxWithTitle:(NSString*)title {
  NSButton* button = [[[NSButton alloc] initWithFrame:NSZeroRect] autorelease];
  [button setButtonType:NSSwitchButton];
  [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTitle:title];
  return button;
}

@end
