// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cursor_accessibility_scale_factor_observer.h"

@implementation CursorAccessibilityScaleFactorObserver {
  void (^_handler)() __strong;
  NSUserDefaults* __strong _defaults;
}

- (instancetype)initWithHandler:(void (^)())handler {
  self = [super init];
  if (self) {
    _handler = handler;
    _defaults =
        [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.universalaccess"];
    [_defaults addObserver:self
                forKeyPath:@"mouseDriverCursorSize"
                   options:0
                   context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [_defaults removeObserver:self forKeyPath:@"mouseDriverCursorSize"];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  _handler();
}
@end
