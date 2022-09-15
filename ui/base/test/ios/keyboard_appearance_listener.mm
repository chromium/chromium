// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/ios/keyboard_appearance_listener.h"

#include <vector>

@implementation KeyboardAppearanceListener {
 @private
  std::vector<id> _notificationObservers;
}

@synthesize keyboardVisible = _keyboardVisible;

- (id)init {
  self = [super init];
  if (self) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    _notificationObservers.push_back(
        [center addObserverForName:UIKeyboardDidShowNotification
                            object:nil
                             queue:nil
                        usingBlock:^(NSNotification* arg) {
                          _keyboardVisible = true;
                        }]);
    _notificationObservers.push_back(
        [center addObserverForName:UIKeyboardWillHideNotification
                            object:nil
                             queue:nil
                        usingBlock:^(NSNotification* arg) {
                          _keyboardVisible = false;
                        }]);
  }
  return self;
}

- (void)dealloc {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  for (const auto& observer : _notificationObservers) {
    [nc removeObserver:observer];
  }
  _notificationObservers.clear();
  [super dealloc];
}
@end
