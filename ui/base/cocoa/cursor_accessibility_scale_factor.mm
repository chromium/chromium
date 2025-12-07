// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cursor_accessibility_scale_factor.h"

#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <optional>

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "ui/base/cursor/cursor.h"

@interface CursorAccessibilityScaleFactorNotifier ()
@property(readonly, nonatomic) float scaleFactor;
@end

@implementation CursorAccessibilityScaleFactorNotifier {
  NSMutableDictionary<id<NSObject>, void (^)(void)>* _observers;
  NSUserDefaults* __strong _defaults;
  float _scaleFactor;
}

@synthesize scaleFactor = _scaleFactor;

+ (CursorAccessibilityScaleFactorNotifier*)sharedNotifier {
  static dispatch_once_t once;
  static CursorAccessibilityScaleFactorNotifier* instance;
  dispatch_once(&once, ^{
    instance = [[CursorAccessibilityScaleFactorNotifier alloc] init];
  });
  return instance;
}

- (instancetype)init {
  if (self = [super init]) {
    CHECK(base::CommandLine::ForCurrentProcess()
              ->GetSwitchValueASCII("type")
              .empty());

    _observers = [[NSMutableDictionary alloc] init];
    _defaults =
        [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.universalaccess"];
    [_defaults addObserver:self
                forKeyPath:@"mouseDriverCursorSize"
                   options:NSKeyValueObservingOptionNew |
                           NSKeyValueObservingOptionInitial
                   context:nullptr];
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  // If this key is present, it will be a number from [1.0, 4.0], but it might
  // not be present if it is never set, or in tests. In that case, this
  // observation method will be called with NSNull, so the following code must
  // handle that case.
  NSNumber* defaultsValue =
      base::apple::ObjCCast<NSNumber>(change[NSKeyValueChangeNewKey]);
  _scaleFactor = std::clamp(defaultsValue.floatValue, 1.0f, 4.0f);
  for (void (^observer)() in _observers.allValues) {
    observer();
  }
}

- (id<NSObject>)addObserver:(void (^)())observer {
  static int sequence;

  // An opaque token is required to represent the observation, something that is
  // quick and easy and compares differently to other values. A wrapped
  // incrementing number will do.
  id<NSObject, NSCopying> token = @(++sequence);
  _observers[token] = observer;

  return token;
}

- (void)removeObserver:(id<NSObject>)token {
  [_observers removeObjectForKey:token];
}

namespace ui {

float GetCursorAccessibilityScaleFactor() {
  return CursorAccessibilityScaleFactorNotifier.sharedNotifier.scaleFactor;
}

}  // namespace ui

@end
