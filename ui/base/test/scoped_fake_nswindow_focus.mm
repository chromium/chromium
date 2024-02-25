// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_nswindow_focus.h"

#import <Cocoa/Cocoa.h>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"

using base::apple::ScopedObjCClassSwizzler;

namespace {

NSWindow* g_fake_focused_window = nil;
base::apple::ScopedObjCClassSwizzler* g_order_out_swizzler = nullptr;

void SetFocus(NSWindow* window) {
  g_fake_focused_window = window;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:g_fake_focused_window];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeKeyNotification
                    object:g_fake_focused_window];
}

void ClearFocus() {
  NSWindow* window = g_fake_focused_window;
  g_fake_focused_window = nil;

  // Some observers of these notifications (e.g. NSRemoteView) expect a
  // non-nil window, and throw an exception otherwise. When the window's
  // nil, just skip posting the notification.
  if (window == nil) {
    return;
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignKeyNotification
                    object:window];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignMainNotification
                    object:window];
}

}  // namespace

// Donates testing implementations of NSWindow methods.
@interface FakeNSWindowFocusDonor : NSObject
@end

@implementation FakeNSWindowFocusDonor

- (BOOL)isKeyWindow {
  NSWindow* selfAsWindow = base::apple::ObjCCastStrict<NSWindow>(self);
  return selfAsWindow == g_fake_focused_window;
}

- (BOOL)isMainWindow {
  NSWindow* selfAsWindow = base::apple::ObjCCastStrict<NSWindow>(self);
  return selfAsWindow == g_fake_focused_window;
}

- (void)makeKeyWindow {
  NSWindow* selfAsWindow = base::apple::ObjCCastStrict<NSWindow>(self);
  if (selfAsWindow == g_fake_focused_window ||
      ![selfAsWindow canBecomeKeyWindow])
    return;

  ClearFocus();
  SetFocus(selfAsWindow);
}

- (void)makeMainWindow {
  [self makeKeyWindow];
}

- (void)orderOut:(id)sender {
  NSWindow* selfAsWindow = base::apple::ObjCCastStrict<NSWindow>(self);
  if (selfAsWindow == g_fake_focused_window)
    ClearFocus();
  g_order_out_swizzler->InvokeOriginal<void, id>(self, _cmd, sender);
}

- (void)resignKeyWindow {
}

- (void)resignMainWindow {
}

@end

namespace ui::test {

ScopedFakeNSWindowFocus::ScopedFakeNSWindowFocus()
    : is_main_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(isMainWindow))),
      make_main_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(makeMainWindow))),
      resign_main_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(resignMainWindow))),
      is_key_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(isKeyWindow))),
      make_key_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(makeKeyWindow))),
      resign_key_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(resignKeyWindow))),
      order_out_swizzler_(
          new ScopedObjCClassSwizzler([NSWindow class],
                                      [FakeNSWindowFocusDonor class],
                                      @selector(orderOut:))) {
  g_order_out_swizzler = order_out_swizzler_.get();
}

ScopedFakeNSWindowFocus::~ScopedFakeNSWindowFocus() {
  g_order_out_swizzler = nullptr;
  ClearFocus();
}

}  // namespace ui::test
