// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/native_window_tracker_cocoa.h"

#import <AppKit/AppKit.h>

#include <memory>

@interface BridgedNativeWindowTracker : NSObject

- (instancetype)initWithNSWindow:(NSWindow*)window;
- (bool)wasNSWindowClosed;
- (void)onWindowWillClose:(NSNotification*)notification;

@end

@implementation BridgedNativeWindowTracker {
  NSWindow* __weak _window;
}

- (instancetype)initWithNSWindow:(NSWindow*)window {
  _window = window;
  [NSNotificationCenter.defaultCenter addObserver:self
                                         selector:@selector(onWindowWillClose:)
                                             name:NSWindowWillCloseNotification
                                           object:_window];
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (bool)wasNSWindowClosed {
  return _window == nil;
}

- (void)onWindowWillClose:(NSNotification*)notification {
  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:_window];
  _window = nil;
}

@end

namespace views {

struct NativeWindowTrackerCocoa::ObjCStorage {
  BridgedNativeWindowTracker* __strong tracker;
};

NativeWindowTrackerCocoa::NativeWindowTrackerCocoa(
    gfx::NativeWindow native_window)
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->tracker = [[BridgedNativeWindowTracker alloc]
      initWithNSWindow:native_window.GetNativeNSWindow()];
}

NativeWindowTrackerCocoa::~NativeWindowTrackerCocoa() = default;

bool NativeWindowTrackerCocoa::WasNativeWindowDestroyed() const {
  return [objc_storage_->tracker wasNSWindowClosed];
}

// static
std::unique_ptr<NativeWindowTracker> NativeWindowTracker::Create(
    gfx::NativeWindow window) {
  return std::make_unique<NativeWindowTrackerCocoa>(window);
}

}  // namespace views
