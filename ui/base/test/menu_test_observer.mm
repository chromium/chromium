// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/menu_test_observer.h"

#include "base/check_op.h"
#import "base/mac/objc_release_properties.h"

@implementation MenuTestObserver

@synthesize menu = _menu;
@synthesize isOpen = _isOpen;
@synthesize didOpen = _didOpen;
@synthesize closeAfterOpening = _closeAfterOpening;
@synthesize openCallback = _openCallback;

- (instancetype)initWithMenu:(NSMenu*)menu {
  if ((self = [super init])) {
    _menu = menu;

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(menuDidBeginTracking:)
                   name:NSMenuDidBeginTrackingNotification
                 object:_menu];
    [center addObserver:self
               selector:@selector(menuDidEndTracking:)
                   name:NSMenuDidEndTrackingNotification
                 object:_menu];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  base::mac::ReleaseProperties(self);
  [super dealloc];
}

- (void)menuDidBeginTracking:(NSNotification*)notif {
  DCHECK_EQ(_menu, [notif object]);
  _isOpen = YES;
  _didOpen = YES;

  // Post the callback to the runloop, since in this notification callback,
  // the menu may not be fully in its tracking mode yet.
  NSArray* modes = @[ NSEventTrackingRunLoopMode, NSDefaultRunLoopMode ];
  [self performSelector:@selector(performOpenTasks)
             withObject:nil
             afterDelay:0
                inModes:modes];
}

- (void)menuDidEndTracking:(NSNotification*)notif {
  DCHECK_EQ(_menu, [notif object]);
  _isOpen = NO;
}

- (void)performOpenTasks {
  DCHECK(_isOpen);

  if (_openCallback)
    _openCallback(self);

  if (_closeAfterOpening)
    [_menu cancelTracking];
}

@end
